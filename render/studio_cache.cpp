#include "stdafx.h"
#include "studio_cache.h"
#include "studio_misc.h"
#include <meshoptimizer.h>
#include "memory.h"

namespace Render
{

// max models cached, models are never freed so this should be large
constexpr int StudioCacheMaxBits = 12; // 4096 entries

static int s_cacheCount;
static StudioCache s_caches[1 << StudioCacheMaxBits];

// studiohdr_t::name
struct NameField
{
    char name[60];
    uint16_t magic; // NameFieldMagic
    uint16_t index; // indexes into s_caches
};

constexpr uint16_t NameFieldMagic = (0 << 0) | (3 << 8);

struct StudioVertexFat
{
    Vector3 position;
    Vector2 texCoord;
    float bone;
    Vector3 normal;
};

struct BuildBuffer
{
    unsigned vertexCount;
    StudioVertexFat *vertices;
    unsigned indexCount;
    GLuint *indices;
};

static unsigned ParseTricmds(BuildBuffer *build,
    short *tricmds,
    Vector3 *vertices,
    Vector3 *normals,
    byte *vertinfo,
    float s,
    float t)
{
    unsigned baseVertex = build->vertexCount;
    unsigned startIndexCount = build->indexCount;

    while (1)
    {
        int value = *tricmds++;
        if (!value)
            break;

        bool trifan = false;

        if (value < 0)
        {
            trifan = true;
            value = -value;
        }

        unsigned count = (unsigned)value;

        unsigned offset = build->vertexCount;
        build->vertexCount += count;

        StudioVertexFat *vert = &build->vertices[offset];

        for (unsigned l = 0; l < count; l++)
        {
            vert->position = vertices[tricmds[0]];
            vert->normal = normals[tricmds[1]];

            vert->texCoord.x = s * tricmds[2];
            vert->texCoord.y = t * tricmds[3];

            vert->bone = vertinfo[tricmds[0]];

            tricmds += 4;
            vert++;
        }

        unsigned indexBase = offset - baseVertex;
        if (trifan)
        {
            for (unsigned i = 2; i < count; i++)
            {
                build->indices[build->indexCount++] = indexBase;
                build->indices[build->indexCount++] = indexBase + i - 1;
                build->indices[build->indexCount++] = indexBase + i;
            }
        }
        else
        {
            for (unsigned i = 2; i < count; i++)
            {
                if (!(i % 2))
                {
                    build->indices[build->indexCount++] = indexBase + i - 2;
                    build->indices[build->indexCount++] = indexBase + i - 1;
                    build->indices[build->indexCount++] = indexBase + i;
                }
                else
                {
                    build->indices[build->indexCount++] = indexBase + i - 1;
                    build->indices[build->indexCount++] = indexBase + i - 2;
                    build->indices[build->indexCount++] = indexBase + i;
                }
            }
        }
    }

    unsigned meshVertexCount = build->vertexCount - baseVertex;
    unsigned meshIndexCount = build->indexCount - startIndexCount;

    // FIXME: why is this still in? doubt it'll improve performance, but profile and remove
    if (meshVertexCount > 0 && meshIndexCount > 0)
    {
        std::vector<unsigned> remap(meshVertexCount);
        size_t uniqueVertexCount = meshopt_generateVertexRemap(
            remap.data(),
            &build->indices[startIndexCount],
            meshIndexCount,
            &build->vertices[baseVertex],
            meshVertexCount,
            sizeof(StudioVertexFat));

        std::vector<unsigned> remappedIndices(meshIndexCount);
        meshopt_remapIndexBuffer(
            remappedIndices.data(),
            &build->indices[startIndexCount],
            meshIndexCount,
            remap.data());

        std::vector<StudioVertexFat> remappedVertices(uniqueVertexCount);
        meshopt_remapVertexBuffer(
            remappedVertices.data(),
            &build->vertices[baseVertex],
            meshVertexCount,
            sizeof(StudioVertexFat),
            remap.data());

        meshopt_optimizeVertexCache(
            remappedIndices.data(),
            remappedIndices.data(),
            meshIndexCount,
            uniqueVertexCount);

        uniqueVertexCount = meshopt_optimizeVertexFetch(
            remappedVertices.data(),
            remappedIndices.data(),
            meshIndexCount,
            remappedVertices.data(),
            uniqueVertexCount,
            sizeof(StudioVertexFat));

        memcpy(&build->vertices[baseVertex], remappedVertices.data(), uniqueVertexCount * sizeof(StudioVertexFat));
        memcpy(&build->indices[startIndexCount], remappedIndices.data(), meshIndexCount * sizeof(unsigned));

        build->vertexCount = baseVertex + (unsigned)uniqueVertexCount;
    }

    return baseVertex;
}

static int CountVertsTricmds(short *tricmds)
{
    int result = 0;

    while (1)
    {
        int value = *tricmds++;
        if (!value)
            break;

        if (value < 0)
            value = -value;

        result += value;

        tricmds += (4 * value);
    }

    return result;
}

static int CountVerts(studiohdr_t *header)
{
    int total_verts = 0;

    mstudiobodyparts_t *bodyparts = (mstudiobodyparts_t *)((byte *)header + header->bodypartindex);

    for (int i = 0; i < header->numbodyparts; i++)
    {
        mstudiobodyparts_t *bodypart = &bodyparts[i];
        mstudiomodel_t *models = (mstudiomodel_t *)((byte *)header + bodypart->modelindex);

        for (int j = 0; j < bodypart->nummodels; j++)
        {
            mstudiomodel_t *submodel = &models[j];
            mstudiomesh_t *meshes = (mstudiomesh_t *)((byte *)header + submodel->meshindex);

            for (int k = 0; k < submodel->nummesh; k++)
            {
                mstudiomesh_t *mesh = &meshes[k];
                short *tricmds = (short *)((byte *)header + mesh->triindex);
                total_verts += CountVertsTricmds(tricmds);
            }
        }
    }

    return total_verts;
}

static void PackIndices(uint32_t *indices, unsigned count)
{
    unsigned i = 0, j = 0;

    while (i + 1 < count)
    {
        uint32_t low = indices[i++];
        uint32_t high = indices[i++];
        indices[j++] = low | (high << 16);
    }

    if (count & 1u)
    {
        indices[j] = indices[i];
    }
}

static void PackNormal(int8_t(&dest)[4], const Vector3 &source)
{
    dest[0] = (int8_t)Lerp(INT8_MIN, INT8_MAX, 0.5f + (source.x * 0.5f));
    dest[1] = (int8_t)Lerp(INT8_MIN, INT8_MAX, 0.5f + (source.y * 0.5f));
    dest[2] = (int8_t)Lerp(INT8_MIN, INT8_MAX, 0.5f + (source.z * 0.5f));
    dest[3] = 0;
}

static void PackVertices(StudioVertexFat *source, int count)
{
    static_assert(sizeof(StudioVertex) < sizeof(StudioVertexFat), "wtf");
    StudioVertex *dest = (StudioVertex *)source;

    for (int i = 0; i < count; i++)
    {
        StudioVertexFat from = source[i];
        StudioVertex &to = dest[i];

        to.position = from.position;
        to.texCoord = from.texCoord;
        to.bone = from.bone;
        PackNormal(to.normal, from.normal);
    }
}

static void BuildStudioVertexBuffer(StudioCache *cache, model_t *model, studiohdr_t *header)
{
    int total_verts = CountVerts(header);

    TempMemoryScope temp;

    BuildBuffer build;
    build.vertexCount = 0;
    build.vertices = temp.Alloc<StudioVertexFat>(total_verts);
    build.indexCount = 0;
    build.indices = temp.Alloc<GLuint>(total_verts * 3);

    mstudiobodyparts_t *bodyparts = (mstudiobodyparts_t *)((byte *)header + header->bodypartindex);

    studiohdr_t *textureheader = studioTextureHeader(model, header);
    short *skins = (short *)((byte *)textureheader + textureheader->skinindex);
    mstudiotexture_t *textures = (mstudiotexture_t *)((byte *)textureheader + textureheader->textureindex);

    cache->bodyparts = memoryStaticAlloc<StudioBodypart>(header->numbodyparts);

    for (int i = 0; i < header->numbodyparts; i++)
    {
        mstudiobodyparts_t *bodypart = &bodyparts[i];
        mstudiomodel_t *models = (mstudiomodel_t *)((byte *)header + bodypart->modelindex);

        StudioBodypart *mem_bodypart = &cache->bodyparts[i];
        mem_bodypart->models = memoryStaticAlloc<StudioSubModel>(bodypart->nummodels);

        for (int j = 0; j < bodypart->nummodels; j++)
        {
            mstudiomodel_t *submodel = &models[j];
            mstudiomesh_t *meshes = (mstudiomesh_t *)((byte *)header + submodel->meshindex);

            Vector3 *vertices = (Vector3 *)((byte *)header + submodel->vertindex);
            Vector3 *normals = (Vector3 *)((byte *)header + submodel->normindex);

            byte *vertinfo = (byte *)((byte *)header + submodel->vertinfoindex);

            StudioSubModel *mem_model = &mem_bodypart->models[j];
            mem_model->meshes = memoryStaticAlloc<StudioMesh>(submodel->nummesh);

            for (int k = 0; k < submodel->nummesh; k++)
            {
                mstudiomesh_t *mesh = &meshes[k];
                mstudiotexture_t *texture = &textures[skins[mesh->skinref]];
                short *tricmds = (short *)((byte *)header + mesh->triindex);

                float s = 1.0f / (float)texture->width;
                float t = 1.0f / (float)texture->height;

                unsigned index_offset = build.indexCount;
                unsigned baseVertex = ParseTricmds(&build, tricmds, vertices, normals, vertinfo, s, t);

                StudioMesh *mem_mesh = &mem_model->meshes[k];
                mem_mesh->indexOffset_notbytes = index_offset;
                mem_mesh->indexCount = build.indexCount - index_offset;
                mem_mesh->baseVertex = baseVertex;
            }
        }
    }

    // packing vertices afterwards
    PackVertices(build.vertices, build.vertexCount);

    // why use u32 when u16 do trick..
    PackIndices(build.indices, build.indexCount);

    glGenBuffers(1, &cache->vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, cache->vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, build.vertexCount * sizeof(StudioVertex), build.vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &cache->indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cache->indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, build.indexCount * sizeof(GLushort), build.indices, GL_STATIC_DRAW);
}

static void BuildStudioCache(StudioCache *cache, model_t *model, studiohdr_t *header)
{
    Q_strcpy(cache->fileName, model->name);
    cache->fileLength = header->length;

    // build vertex and index buffer
    BuildStudioVertexBuffer(cache, model, header);
}

static NameField *GetNameField(studiohdr_t *header)
{
    return reinterpret_cast<NameField *>(header->name);
}

static int GetCacheIndex(studiohdr_t *header)
{
    NameField *field = GetNameField(header);
    if (field->magic != NameFieldMagic)
    {
        return -1;
    }

    return field->index;
}

static void SetCacheIndex(studiohdr_t *header, int index)
{
    NameField *field = GetNameField(header);
    field->magic = NameFieldMagic;
    field->index = static_cast<uint16_t>(index);
}

static void ReleaseCache(StudioCache *cache)
{
    // we can't free the memory allocated with memoryStaticAlloc, but
    // those allocations are very small so doesn't matter
    // free the gpu buffers though
    glDeleteBuffers(1, &cache->vertexBuffer);
    glDeleteBuffers(1, &cache->indexBuffer);
    memset(cache, 0, sizeof(*cache));
}

StudioCache *studioCacheGet(model_t *model, studiohdr_t *header)
{
    // see if the cache index is in the header
    int cacheIndex = GetCacheIndex(header);
    if (cacheIndex != -1)
    {
        return &s_caches[cacheIndex];
    }

    // it was not, fuck
    GL3_ASSERT(model->name[0]);

    uint32_t hash = HashString(model->name);
    uint32_t mask = (1 << StudioCacheMaxBits) - 1;
    uint32_t step = (hash >> (32 - StudioCacheMaxBits)) | 1;

    for (int i = (hash + step) & mask;; i = (i + step) & mask)
    {
        StudioCache *cache = &s_caches[i];
        if (!cache->fileName[0])
        {
            if (s_cacheCount + 1 >= (1 << StudioCacheMaxBits))
            {
                platformError("Studio model cache full");
            }

            s_cacheCount++;

            BuildStudioCache(cache, model, header);
            SetCacheIndex(header, i);
            return cache;
        }

        if (!strcmp(model->name, cache->fileName))
        {
            // see if the model has changed (flush command)
            if (header->length != cache->fileLength)
            {
                ReleaseCache(cache);
                BuildStudioCache(cache, model, header);
            }

            // update the header
            SetCacheIndex(header, i);
            return cache;
        }
    }
}

StudioCache *studioCacheGet(cl_entity_t *entity)
{
    model_t *model = entity->model;
    if (!model)
        return nullptr;

    if (model->type != mod_studio)
        return nullptr;

    studiohdr_t *studiohdr = static_cast<studiohdr_t *>(g_engineStudio.Mod_Extradata(model));
    if (!studiohdr)
        return nullptr;

    return studioCacheGet(model, studiohdr);
}

void studioCacheTouchAll()
{
    for (int i = 2;; i++)
    {
        model_t *model = g_engineStudio.GetModelByIndex(i);
        if (!model)
        {
            break;
        }

        if (model->type != mod_studio)
        {
            continue;
        }

        studiohdr_t *studiohdr = static_cast<studiohdr_t *>(g_engineStudio.Mod_Extradata(model));
        if (studiohdr)
        {
            studioCacheGet(model, studiohdr);
        }
    }
}

}
