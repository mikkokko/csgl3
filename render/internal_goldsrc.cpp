#include "stdafx.h"
#include "internal.h"
#include "model_goldsrc.h"
#include "random.h"
#include "lightstyle.h"
#include "texture.h"
#include "memory.h"
#include "brush.h"
#include "decalclip.h"
#include "triapigl3.h"

// warning: this file sucks

namespace Render
{

enum class SurfaceType
{
    Unspecified,
    Normal,
    HL25
};

// tries to validate an msurface_t without dereferencing pointers
// this is an overkill (would get away with just the plane check), but we want to be sure
template<typename S>
static bool ValidateSurface(const goldsrc::model_t *model, const S *surface)
{
    // visframe (can be anything if we're switching between renderers during runtime)

    // plane
    int planeIndex = surface->plane - model->planes;
    if (planeIndex < 0 || planeIndex >= model->numplanes)
    {
        return false;
    }

    // flags, 54 is a randomly generated number
    if (surface->flags < 0 || surface->flags > 54)
    {
        return false;
    }

    // firstedge and numedges, these go out of bounds even with valid bsps?

    // texturemins and extents should be evenly divisible by 16
    for (int i = 0; i < 2; i++)
    {
        if ((surface->texturemins[i] % 16) != 0)
        {
            return false;
        }

        if ((surface->extents[i] % 16) != 0)
        {
            return false;
        }
    }

    // light_s and light_t should be within the lightmap atlas (128x128)
    if (surface->light_s < 0 || surface->light_s >= 128
        || surface->light_t < 0 || surface->light_t >= 128)
    {
        return false;
    }

    // polys, can't check without dereferencing
    // texturechain, should be null but don't count on it...

    // texinfo
    int texInfoIndex = surface->texinfo - model->texinfo;
    if (texInfoIndex < 0 || texInfoIndex >= model->numtexinfo)
    {
        return false;
    }

    // dlightframe, can't test if switching renderers
    // dlightbits, can't test if switching renderers

    // lightmaptexturenum is [0, 64[
    if (surface->lightmaptexturenum < 0 || surface->lightmaptexturenum >= 64)
    {
        return false;
    }

    // samples, can't properly test (we don't know the bounds)
    // decals, can't test

    // i guess it aight
    return true;
}

template<typename S>
static bool ValidateSurfaces(const goldsrc::model_t *model)
{
    S *surfaces = reinterpret_cast<S *>(model->surfaces);

    for (int i = 0; i < model->numsurfaces; i++)
    {
        if (!ValidateSurface(model, &surfaces[i]))
        {
            return false;
        }
    }

    return true;
}

static SurfaceType GetSurfaceType(const goldsrc::model_t *model)
{
    static SurfaceType surfaceType = SurfaceType::Unspecified;
    if (surfaceType != SurfaceType::Unspecified)
    {
        return surfaceType;
    }

    if (ValidateSurfaces<goldsrc::msurface_t>(model))
    {
        surfaceType = SurfaceType::Normal;
        return surfaceType;
    }

    if (ValidateSurfaces<goldsrc::msurface_new_t>(model))
    {
        surfaceType = SurfaceType::HL25;
        return surfaceType;
    }

    platformError("Failed to determine msurface_t type\n");
}

static const goldsrc::msurface_t *GetSurface(const goldsrc::model_t *model, int index)
{
    GL3_ASSERT(index >= 0 && index < model->numsurfaces);

    if (GetSurfaceType(model) == SurfaceType::HL25)
    {
        const goldsrc::msurface_new_t *newSurfaces = reinterpret_cast<const goldsrc::msurface_new_t *>(model->surfaces);
        return reinterpret_cast<const goldsrc::msurface_t *>(&newSurfaces[index]);
    }

    return &model->surfaces[index];
}

// returns -1 on all error cases
static int SurfaceIndex(const goldsrc::model_t *model, goldsrc::msurface_t *surface)
{
    int index;

    if (GetSurfaceType(model) == SurfaceType::HL25)
    {
        index = reinterpret_cast<goldsrc::msurface_new_t *>(surface) - reinterpret_cast<goldsrc::msurface_new_t *>(model->surfaces);
    }
    else
    {
        index = surface - model->surfaces;
    }

    if (index < 0 || index >= model->numsurfaces)
    {
        return -1;
    }

    return index;
}

// fake random tiling textures
// we take all of the textures from -0 to -9 and make
// an atlas out of them, not really random but it's
// simple, and looks better than having no randomization
// at all or doing it the engine opengl way...

static constexpr int TiledTextureGridSize(int textureCount)
{
    // we want the grid to be square and large enough to fit all of the textures in
    // we also want it to be a power of 2 (so power of 2 textures won't need to be rescaled)
    int gridSize = 1;

    while (gridSize * gridSize < textureCount)
    {
        gridSize *= 2;
    }

    return gridSize;
}

constexpr int MaxTiledTextures = 10; // 0 to 9
constexpr int MaxGridSize = TiledTextureGridSize(MaxTiledTextures);
constexpr int MaxTileCount = MaxGridSize * MaxGridSize;

static goldsrc::texture_t *FindTextureByName(goldsrc::texture_t **textures, int num_textures, const char *name)
{
    for (int i = 0; i < num_textures; i++)
    {
        goldsrc::texture_t *texture = textures[i];
        if (!texture)
        {
            continue;
        }

        if (!Q_strcasecmp(texture->name, name))
        {
            return texture;
        }
    }

    return nullptr;
}

// sloppy error checking, but it should always work
static bool GetTextureSize(GLuint textureName, int &width, int &height)
{
    GL3_ASSERT(textureName);

    width = 0;
    height = 0;

    // we need to get the size from the opengl texture name
    // because it might have been downscaled before upload
    glBindTexture(GL_TEXTURE_2D, textureName);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

    return (width > 0 && height > 0);
}

template<typename T>
static void ShuffleArray(T *array, int count)
{
    for (int i = count - 1; i > 0; i--)
    {
        int j = randomInt(0, i);

        T temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

static GLuint MakeTiledTextureAtlas(const char (&name)[16], GLuint *textures, int singleWidth, int singleHeight, int gridWidth, int gridHeight)
{
    int atlasWidth = gridWidth * singleWidth;
    int atlasHeight = gridHeight * singleHeight;

    GL_ERRORS();

    GLuint atlas = textureAllocateAndBind(GL_TEXTURE_2D, name, true);

    GL_ERRORS();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GL_ERRORS();

    GLuint readFramebuffer, drawFramebuffer;
    glGenFramebuffers(1, &readFramebuffer);
    glGenFramebuffers(1, &drawFramebuffer);

    GL_ERRORS();

    GLint saveRead, saveDraw;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &saveRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &saveDraw);

    GL_ERRORS();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);

    GL_ERRORS();

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, atlas, 0);

    GL_ERRORS();

    GL3_ASSERT(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    GL_ERRORS();

    for (int y = 0; y < gridHeight; y++)
    {
        for (int x = 0; x < gridWidth; x++)
        {
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[y * gridWidth + x], 0);

            GL_ERRORS();

            GL3_ASSERT(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

            int xOffset = x * singleWidth;
            int yOffset = y * singleHeight;

            glBlitFramebuffer(0, 0, singleWidth, singleHeight,
                xOffset, yOffset, xOffset + singleWidth, yOffset + singleHeight,
                GL_COLOR_BUFFER_BIT,
                GL_NEAREST);

            GL_ERRORS();
        }
    }

    GL_ERRORS();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, saveRead);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, saveDraw);

    GL_ERRORS();

    glDeleteFramebuffers(1, &readFramebuffer);
    glDeleteFramebuffers(1, &drawFramebuffer);

    GL_ERRORS();

    glGenerateMipmap(GL_TEXTURE_2D);

    GL_ERRORS();

    return atlas;
}

static GLuint LoadTiledTexture(goldsrc::texture_t **textures, int num_textures, const goldsrc::texture_t *firstTexture, int &fullWidth, int &fullHeight)
{
    int tiledTextureCount = 0;
    GLuint tiledTextures[MaxTileCount];

    int singleWidth, singleHeight;
    if (!GetTextureSize(firstTexture->gl_texturenum, singleWidth, singleHeight))
    {
        GL3_ASSERT(0);
        return 0; // what the fuck
    }

    tiledTextures[tiledTextureCount++] = firstTexture->gl_texturenum;

    for (char digit = '1'; digit <= '9'; digit++)
    {
        char name[16];
        Q_strcpy(name, firstTexture->name);
        name[1] = digit;

        const goldsrc::texture_t *texture = FindTextureByName(textures, num_textures, name);
        if (!texture)
        {
            break;
        }

        // size schizo check, not sure how software renderer handles these
        int secondWidth, secondHeight;
        if (!GetTextureSize(texture->gl_texturenum, secondWidth, secondHeight))
        {
            GL3_ASSERT(false);
            break;
        }

        if (secondWidth != singleWidth || secondHeight != singleHeight)
        {
            GL3_ASSERT(false);
            break;
        }

        tiledTextures[tiledTextureCount++] = texture->gl_texturenum;
    }

    int tileCountSquared = TiledTextureGridSize(tiledTextureCount);
    int gridWidth = tileCountSquared;
    int gridHeight = tileCountSquared;

    fullWidth = singleWidth * gridWidth;
    fullHeight = singleHeight * gridHeight;

    // check if it's already loaded
    // FIXME: we could do this earlier, but we need to fill fullWidth and fullHeight
    GLuint check = textureFind(GL_TEXTURE_2D, firstTexture->name);
    if (check)
    {
        return check;
    }

    int tileCount = tileCountSquared * tileCountSquared;

    GL3_ASSERT(tileCount <= MaxTileCount);
    GL3_ASSERT(tileCount >= tiledTextureCount);

    // if there were not enough unique textures, duplicate the rest
    if (tileCount > tiledTextureCount)
    {
        int addCount = tileCount - tiledTextureCount;

        for (int j = 0; j < addCount; j++)
        {
            int index = randomInt(0, tiledTextureCount - 1);
            tiledTextures[tiledTextureCount++] = tiledTextures[index];
        }
    }

    GL3_ASSERT(tiledTextureCount == tileCount);

    ShuffleArray(tiledTextures, tiledTextureCount);

    return MakeTiledTextureAtlas(firstTexture->name, tiledTextures, singleWidth, singleHeight, gridWidth, gridHeight);
}

static int LookupTextureIndex(const goldsrc::model_t &model, const goldsrc::texture_t *texture)
{
    for (int i = 0; i < model.numtextures; i++)
    {
        if (model.textures[i] == texture)
        {
            return i;
        }
    }

    GL3_ASSERT(false);
    return -1;
}

static void LoadTextures(const goldsrc::model_t &engineModel, gl3_worldmodel_t &model)
{
    model.numtextures = engineModel.numtextures;
    model.textures = memoryLevelAlloc<gl3_texture_t>(model.numtextures);

    for (int i = 0; i < model.numtextures; i++)
    {
        const goldsrc::texture_t *source = engineModel.textures[i];
        if (!source)
        {
            GL3_ASSERT(false);
            continue;
        }

        gl3_texture_t *dest = &model.textures[i];

        Q_strcpy(dest->name, source->name);
        dest->width = source->width;
        dest->height = source->height;

        // check for tiling textures
        if (source->name[0] == '-')
        {
            if (source->name[1] == '0')
            {
                int full_width, full_height;
                GLuint texture = LoadTiledTexture(engineModel.textures, engineModel.numtextures, source, full_width, full_height);
                if (texture)
                {
                    dest->gl_texturenum = texture;
                    dest->width = full_width;
                    dest->height = full_height;
                }
            }
        }
        else
        {
            dest->gl_texturenum = source->gl_texturenum;
        }

        dest->anim_total = source->anim_total;
        dest->anim_min = source->anim_min;
        dest->anim_max = source->anim_max;

        if (source->anim_next)
        {
            int texture_index = LookupTextureIndex(engineModel, source->anim_next);
            if (texture_index != -1)
            {
                dest->anim_next = &model.textures[texture_index];
            }
        }

        if (source->alternate_anims)
        {
            int texture_index = LookupTextureIndex(engineModel, source->alternate_anims);
            if (texture_index != -1)
            {
                dest->alternate_anims = &model.textures[texture_index];
            }
        }
    }

    // update rest of the tiling textures
    for (int i = 0; i < model.numtextures; i++)
    {
        auto &texture = model.textures[i];
        if (texture.name[0] != '-' || texture.name[1] == '0')
            continue;

        GL3_ASSERT(texture.name[1] != '0');

        // find the base texture
        char name[16];
        Q_strcpy(name, texture.name);
        name[1] = '0';

        for (int j = 0; j < model.numtextures; j++)
        {
            auto &other = model.textures[j];
            if (Q_strcasecmp(other.name, name))
                continue;

            // found it
            GL3_ASSERT(!texture.gl_texturenum);
            texture.gl_texturenum = other.gl_texturenum;
            texture.width = other.width;
            texture.height = other.height;
            break;
        }
    }
}

static void LoadPlanes(const goldsrc::model_t &engineModel, gl3_worldmodel_t &model)
{
    model.numplanes = engineModel.numplanes;
    model.planes = memoryLevelAlloc<gl3_plane_t>(model.numplanes);

    for (int i = 0; i < model.numplanes; i++)
    {
        model.planes[i].normal = engineModel.planes[i].normal;
        model.planes[i].dist = engineModel.planes[i].dist;
        model.planes[i].type = engineModel.planes[i].type;
    }
}

static void LoadTexinfo(const goldsrc::model_t &engineModel, gl3_worldmodel_t &model)
{
    model.numtexinfo = engineModel.numtexinfo;
    model.texinfo = memoryLevelAlloc<gl3_texinfo_t>(model.numtexinfo);

    for (int i = 0; i < model.numtexinfo; i++)
    {
        int texture_index = LookupTextureIndex(engineModel, engineModel.texinfo[i].texture);
        if (texture_index != -1)
        {
            model.texinfo[i].texture = &model.textures[texture_index];
        }
    }
}

bool LightmapWideTall(const goldsrc::msurface_t *surface, int &width, int &height)
{
    if (surface->flags & SURF_SKY)
    {
        return false;
    }

    if (surface->flags & SURF_WATER)
    {
        return false;
    }

    if ((surface->flags & SURF_SCROLL) && (surface->texinfo->flags & TEX_SPECIAL))
    {
        return false;
    }

    width = (surface->extents[0] >> 4) + 1;
    height = (surface->extents[1] >> 4) + 1;
    return true;
}

static int FaceVertexCount(const goldsrc::msurface_t *surface)
{
#if 0 // actually don't, we draw skies using surfaces now
    if (surface->flags & SURF_SKY)
    {
        // need to cull sky faces
        return 0;
    }
#endif

    return surface->numedges;
}

static void LoadFaces(const goldsrc::model_t &engineModel, gl3_worldmodel_t &model)
{
    model.numsurfaces = engineModel.numsurfaces;
    model.surfaces = memoryLevelAlloc<gl3_surface_t>(model.numsurfaces);

    for (int i = 0; i < model.numsurfaces; i++)
    {
        const goldsrc::msurface_t *source = GetSurface(&engineModel, i);
        gl3_surface_t *dest = &model.surfaces[i];

        dest->flags = source->flags;
        dest->numverts = FaceVertexCount(source);
        dest->texturechain = nullptr;

        int plane_index = source->plane - engineModel.planes;
        GL3_ASSERT(plane_index >= 0 && plane_index < engineModel.numplanes);
        dest->plane = &model.planes[plane_index];

        int texinfo_index = source->texinfo - engineModel.texinfo;
        GL3_ASSERT(texinfo_index >= 0 && texinfo_index < engineModel.numtexinfo);
        dest->texinfo = &model.texinfo[texinfo_index];

        // keep going until we hit the terminator
        int style_count = 0;

        for (int j = 0; j < MAXLIGHTMAPS; j++)
        {
            if (source->styles[j] == 255)
            {
                break;
            }

            // we use lightstyle 63 as the null lightstyle so make sure we write nothing higher than that
            GL3_ASSERT(source->styles[j] < NULL_LIGHTSTYLE);

            dest->styles[j] = Q_min(source->styles[j], (byte)NULL_LIGHTSTYLE);
            style_count++;
        }

        // set null values, they're nonzero
        for (int j = style_count; j < MAXLIGHTMAPS; j++)
        {
            dest->styles[j] = NULL_LIGHTSTYLE;
        }

        // lightmap stuff (FIXME: we could omit even more surfaces from lightmap building)
        if (source->samples && style_count
            && LightmapWideTall(source, dest->lightmap_width, dest->lightmap_height))
        {
            dest->style_count = style_count;
            dest->lightmap_data = reinterpret_cast<Color24 *>(source->samples);
        }
    }
}

static void LoadMarksurfaces(const goldsrc::model_t &engineModel, gl3_worldmodel_t &model)
{
    model.nummarksurfaces = engineModel.nummarksurfaces;
    model.marksurfaces = memoryLevelAlloc<gl3_surface_t *>(model.nummarksurfaces);

    for (int i = 0; i < model.nummarksurfaces; i++)
    {
        int surface_index = SurfaceIndex(&engineModel, engineModel.marksurfaces[i]);
        if (surface_index == -1)
        {
            //g_engfuncs.Con_Printf("Out of bounds surfce %d (out of %d), NULLing marksurface\n", i, model.nummarksurfaces);
            model.marksurfaces[i] = NULL;
            continue;
        }

        model.marksurfaces[i] = &model.surfaces[surface_index];
    }
}

static void LoadLeafs(const goldsrc::model_t &engineModel, gl3_worldmodel_t &model)
{
    // bruh moment: model->numleafs gets stomped in Mod_LoadBrushModel so we have no way of knowing
    // how many leaves this model actually has. try to guess the amount...
    int approx_numleafs = 0;

    // this looks creepy, but i'm sure it's fine...
    for (int i = engineModel.numleafs - 1;; i++)
    {
        goldsrc::mleaf_t *leaf = &engineModel.leafs[i];

        // check minmaxs as a fallback
        bool valid = true;

        for (int j = 0; j < 3; j++)
        {
            float min = (float)(short)leaf->mins.Get(j);
            float max = (float)(short)leaf->maxs.Get(j);

            if (min != leaf->mins.Get(j) || max != leaf->maxs.Get(j))
            {
                valid = false;
                break;
            }
        }

        if (!valid)
        {
            approx_numleafs = i;
            break;
        }

        // check contents
        if (leaf->contents < -15 || leaf->contents >= 0)
        {
            approx_numleafs = i;
            break;
        }
    }

    model.numleafs = engineModel.numleafs;
    model.numleafs_total = approx_numleafs;
    model.leafs = memoryLevelAlloc<gl3_leaf_t>(model.numleafs_total);

    for (int i = 0; i < model.numleafs_total; i++)
    {
        GL3_ASSERT((engineModel.leafs[i].contents >= -15 && engineModel.leafs[i].contents < 0));

        model.leafs[i].contents = engineModel.leafs[i].contents;

        model.leafs[i].mins = engineModel.leafs[i].mins;
        model.leafs[i].maxs = engineModel.leafs[i].maxs;

        model.leafs[i].compressed_vis = engineModel.leafs[i].compressed_vis; // don't make a copy

        if (engineModel.leafs[i].nummarksurfaces)
        {
            int firstmarksurface_index = engineModel.leafs[i].firstmarksurface - engineModel.marksurfaces;
            GL3_ASSERT(firstmarksurface_index >= 0 && firstmarksurface_index < engineModel.nummarksurfaces);

            model.leafs[i].firstmarksurface = &model.marksurfaces[firstmarksurface_index];
            model.leafs[i].nummarksurfaces = engineModel.leafs[i].nummarksurfaces;

            for (int j = 0; j < model.leafs[i].nummarksurfaces; j++)
            {
                gl3_surface_t *surface = model.leafs[i].firstmarksurface[j];
                if (surface && surface->numverts)
                {
                    model.leafs[i].has_visible_surfaces = true;
                    break;
                }
            }
        }
    }
}

static void SetParent(gl3_node_t *node, gl3_node_t *parent)
{
    node->parent = parent;

    if (node->contents < 0)
        return;

    SetParent(node->children[0], node);
    SetParent(node->children[1], node);
}

static void LoadNodes(const goldsrc::model_t &engineModel, gl3_worldmodel_t &model)
{
    model.numnodes = engineModel.numnodes;
    model.nodes = memoryLevelAlloc<gl3_node_t>(model.numnodes);

    for (int i = 0; i < model.numnodes; i++)
    {
        model.nodes[i].contents = engineModel.nodes[i].contents;

        model.nodes[i].mins = engineModel.nodes[i].mins;
        model.nodes[i].maxs = engineModel.nodes[i].maxs;

        int plane_index = engineModel.nodes[i].plane - engineModel.planes;
        GL3_ASSERT(plane_index >= 0 && plane_index < engineModel.numplanes);
        model.nodes[i].plane = &model.planes[plane_index];

        for (int j = 0; j < 2; j++)
        {
            if (!engineModel.nodes[i].children[j])
            {
                GL3_ASSERT(false);
                continue;
            }

            int node_index = engineModel.nodes[i].children[j] - engineModel.nodes;
            int leaf_index = (goldsrc::mleaf_t *)engineModel.nodes[i].children[j] - engineModel.leafs;

            if (node_index >= 0 && node_index < engineModel.numnodes)
            {
                model.nodes[i].children[j] = &model.nodes[node_index];
            }
            else if (leaf_index >= 0 && leaf_index < model.numleafs_total) // note how we use model.numleafs_total!!! see leaf loading code
            {
                model.nodes[i].children[j] = (gl3_node_t *)&model.leafs[leaf_index];
            }
            else
            {
                GL3_ASSERT(false);
            }
        }
    }

    SetParent(model.nodes, nullptr);
}

bool internalLoadBrushModel(model_t *model, gl3_worldmodel_t *outModel)
{
    outModel->engine_model = model;
    const goldsrc::model_t &src = *reinterpret_cast<const goldsrc::model_t *>(model);

    LoadTextures(src, *outModel);
    LoadPlanes(src, *outModel);
    LoadTexinfo(src, *outModel);
    LoadFaces(src, *outModel);
    LoadMarksurfaces(src, *outModel);
    LoadLeafs(src, *outModel);
    LoadNodes(src, *outModel);

    return true;
}

gl3_brushvert_t *internalBuildVertexBuffer(model_t *model, gl3_worldmodel_t *outModel, int &vertexCount, TempMemoryScope &temp)
{
    const goldsrc::model_t &engineModel = *reinterpret_cast<const goldsrc::model_t *>(model);

    // count vertices in this model
    int num_verts = 0;

    for (int j = 0; j < engineModel.numsurfaces; j++)
    {
        const goldsrc::msurface_t *surface = GetSurface(&engineModel, j);
        if (!FaceVertexCount(surface))
            continue;

        num_verts += surface->numedges;
    }

    outModel->index_size = (num_verts > UINT16_MAX) ? 4 : 2;

    // temporary memory for vbo contents
    gl3_brushvert_t *vertex_buffer = temp.Alloc<gl3_brushvert_t>(num_verts);

    // same loop again for populating the vertex buffer
    int vert_offset = 0;

    for (int j = 0; j < engineModel.numsurfaces; j++)
    {
        const goldsrc::msurface_t *surface = GetSurface(&engineModel, j);
        if (!FaceVertexCount(surface))
        {
            continue;
        }

        const goldsrc::mtexinfo_t *texinfo = surface->texinfo;

        int texture_width = texinfo->texture->width;
        int texture_height = texinfo->texture->height;

        if (texinfo->texture->name[0] == '-')
        {
            // composite tiled texture, different width/height
            GL3_ASSERT(texinfo->texture->anim_total >= 0 && texinfo->texture->anim_total <= 10);
            int size = TiledTextureGridSize(texinfo->texture->anim_total);
            texture_width *= size;
            texture_height *= size;
        }

        outModel->surfaces[j].firstvert = vert_offset;

        // compute in-memory mini per surface index buffers
        auto dest = &outModel->surfaces[j];
        if (dest->numverts)
        {
            dest->numindices = (dest->numverts - 2) * 3;
            if (outModel->index_size == 4)
            {
                uint32_t *indices = memoryLevelAlloc<uint32_t>(dest->numindices);
                dest->indices = indices;

                for (int i = 1; i < dest->numverts - 1; i++)
                {
                    indices[0] = dest->firstvert;
                    indices[1] = dest->firstvert + i;
                    indices[2] = dest->firstvert + i + 1;
                    indices += 3;
                }
            }
            else
            {
                uint16_t *indices = memoryLevelAlloc<uint16_t>(dest->numindices);
                dest->indices = indices;

                for (int i = 1; i < dest->numverts - 1; i++)
                {
                    indices[0] = static_cast<uint16_t>(dest->firstvert);
                    indices[1] = static_cast<uint16_t>(dest->firstvert + i);
                    indices[2] = static_cast<uint16_t>(dest->firstvert + i + 1);
                    indices += 3;
                }
            }
        }

        for (int k = 0; k < surface->numedges; k++)
        {
            int vertex_num;

            int surfedge = engineModel.surfedges[surface->firstedge + k];

            if (surfedge <= 0)
                vertex_num = engineModel.edges[-surfedge].v[1];
            else
                vertex_num = engineModel.edges[surfedge].v[0];

            const goldsrc::mvertex_t *vertex = &engineModel.vertexes[vertex_num];
            float s = Dot(vertex->position, texinfo->vec_s) + texinfo->dist_s;
            float t = Dot(vertex->position, texinfo->vec_t) + texinfo->dist_t;

            // set the lightmap width later
            vertex_buffer[vert_offset + k].position = { vertex->position, 0 };

            vertex_buffer[vert_offset + k].texCoord.x = s / texture_width;
            vertex_buffer[vert_offset + k].texCoord.y = t / texture_height;

            // lightmap texcoords are computed in lightmapCreateAtlas when we know the atlas size
            vertex_buffer[vert_offset + k].texCoord.z = (s - surface->texturemins[0]);
            vertex_buffer[vert_offset + k].texCoord.w = (t - surface->texturemins[1]);

            for (int style = 0; style < MAXLIGHTMAPS; style++)
            {
                vertex_buffer[vert_offset + k].styles[style] = surface->styles[style];
            }
        }

        vert_offset += surface->numedges;
    }

    GL3_ASSERT(vert_offset == num_verts);
    vertexCount = num_verts;
    return vertex_buffer;
}

template struct DecalClip<goldsrc::glvert_t>;

static const goldsrc::glvert_t *ClipDecal(const goldsrc::decal_t *decal, const goldsrc::msurface_t *surface, const goldsrc::texture_t *decalTexture, int &clippedCount)
{
    // FIXME: could move to stack later...
    constexpr int MaxDecalVertices = 32; // overkill, but matches goldsrc
    static goldsrc::glvert_t temp1[MaxDecalVertices];
    static goldsrc::glvert_t temp2[MaxDecalVertices];

    float xScale = (surface->texinfo->texture->width * decal->scale) / decalTexture->width;
    float yScale = (surface->texinfo->texture->height * decal->scale) / decalTexture->height;

    for (int i = 0; i < surface->polys->numverts; i++)
    {
        const goldsrc::glvert_t &vertex = surface->polys->verts[i];

        temp1[i].position = vertex.position;
        temp1[i].texcoord.x = (vertex.texcoord.x - decal->dx) * xScale;
        temp1[i].texcoord.y = (vertex.texcoord.y - decal->dy) * yScale;

        if (decal->flags & 8)
        {
            temp1[i].texcoord.x = 1.0f - temp1[i].texcoord.x;
        }

        if (decal->flags & 16)
        {
            temp1[i].texcoord.y = 1.0f - temp1[i].texcoord.y;
        }
    }

    clippedCount = surface->polys->numverts;
    DecalClip<goldsrc::glvert_t>::Clip(temp1, clippedCount, temp2, clippedCount, ClipEdgeLeft);
    DecalClip<goldsrc::glvert_t>::Clip(temp2, clippedCount, temp1, clippedCount, ClipEdgeRight);
    DecalClip<goldsrc::glvert_t>::Clip(temp1, clippedCount, temp2, clippedCount, ClipEdgeTop);
    DecalClip<goldsrc::glvert_t>::Clip(temp2, clippedCount, temp1, clippedCount, ClipEdgeBottom);

    return temp1;
}

void *internalSurfaceDecals(model_t *_model, void *lastDecal, int surfaceIndex, GLuint *texture, gl3_brushvert_t *vertices, int *vertexCount)
{
    goldsrc::model_t *model = reinterpret_cast<goldsrc::model_t *>(_model);
    GL3_ASSERT(model);

    if (surfaceIndex < 0 || surfaceIndex >= model->numsurfaces)
    {
        GL3_ASSERT(false);
        return nullptr;
    }

    const goldsrc::msurface_t *surface = GetSurface(model, surfaceIndex);

    goldsrc::decal_t *decal = nullptr;
    if (lastDecal)
    {
        decal = static_cast<goldsrc::decal_t *>(lastDecal);
        decal = decal->pnext;
    }
    else
    {
        decal = surface->pdecals;
    }

    if (!decal)
    {
        // no more decals
        return nullptr;
    }

    goldsrc::texture_t *decalTexture = static_cast<goldsrc::texture_t *>(platformGetDecalTexture(decal->texture));
    *texture = decalTexture->gl_texturenum;

    // FIXME: no error handling at all? well the engine also rawdogs this so it should be fine
    const goldsrc::glvert_t *clippedVertices = ClipDecal(decal, surface, decalTexture, *vertexCount);

    // NOTE: only set position and texcoords here!!!
    // caller will set lightmap width and lightstyles
    const goldsrc::mtexinfo_t *texinfo = surface->texinfo;

    for (int i = 0; i < *vertexCount; i++)
    {
        vertices[i].position.x = clippedVertices[i].position.x;
        vertices[i].position.y = clippedVertices[i].position.y;
        vertices[i].position.z = clippedVertices[i].position.z;

        vertices[i].texCoord.x = clippedVertices[i].texcoord.x;
        vertices[i].texCoord.y = clippedVertices[i].texcoord.y;

        float s = Dot((Vector3 &)vertices[i], texinfo->vec_s) + texinfo->dist_s;
        float t = Dot((Vector3 &)vertices[i], texinfo->vec_t) + texinfo->dist_t;

        vertices[i].texCoord.z = (s - surface->texturemins[0]);
        vertices[i].texCoord.w = (t - surface->texturemins[1]);
    }

    return decal;
}

void internalClearBoundTexture()
{
    // frame fields can all be zeroes
    goldsrc::mspriteframe_t frame{};

    goldsrc::msprite_t sprite{};
    sprite.numframes = 1;
    sprite.frames[0].type = goldsrc::SPR_SINGLE;
    sprite.frames[0].frameptr = &frame;

    model_t model{};
    model.cache.data = &sprite;

    // call into engine triapi instead of our shitty gl3 crap
    g_triapiGL1.SpriteTexture(&model, 0);
}

static goldsrc::mspriteframe_t *GetSpriteFrame(goldsrc::msprite_t *sprite, int frame)
{
    if (!sprite->numframes)
    {
        return nullptr;
    }

    if (frame < 0 || frame >= sprite->numframes)
    {
        frame = 0;
    }

    if (sprite->frames[frame].type != goldsrc::SPR_SINGLE)
    {
        return nullptr;
    }

    return sprite->frames[frame].frameptr;
}

bool internalGetSpriteInfo(model_t *model, int frameIndex, SpriteInfo *result)
{
    GL3_ASSERT(model && model->type == mod_sprite);
    goldsrc::msprite_t *sprite = (goldsrc::msprite_t *)model->cache.data;
    if (!sprite)
    {
        GL3_ASSERT(false);
        return false;
    }

    goldsrc::mspriteframe_t *frame = GetSpriteFrame(sprite, frameIndex);
    if (!frame)
    {
        GL3_ASSERT(false);
        return false;
    }

    result->type = sprite->type;
    result->up = frame->up;
    result->down = frame->down;
    result->left = frame->left;
    result->right = frame->right;
    result->texture = frame->gl_texturenum;

    return true;
}

static const goldsrc::msurface_t *TraceLineToSurface(goldsrc::model_t *model, goldsrc::mnode_t *node, const Vector3 &start, const Vector3 &end)
{
    while (true)
    {
        if (node->contents < 0)
        {
            return nullptr;
        }

        float dist1 = Dot(start, node->plane->normal) - node->plane->dist;
        float dist2 = Dot(end, node->plane->normal) - node->plane->dist;

        int side = (dist1 < 0);

        if ((dist2 < 0) == side)
        {
            node = node->children[side];
            continue;
        }

        float frac = dist1 / (dist1 - dist2);
        Vector3 point = VectorLerp(start, end, frac);

        const goldsrc::msurface_t *surface = TraceLineToSurface(model, node->children[side], start, point);
        if (surface)
        {
            return surface;
        }

        for (int i = 0; i < node->numsurfaces; i++)
        {
            surface = GetSurface(model, node->firstsurface + i);

            int s = (int)(Dot(point, surface->texinfo->vec_s) + surface->texinfo->dist_s);
            int t = (int)(Dot(point, surface->texinfo->vec_t) + surface->texinfo->dist_t);
            if (s < surface->texturemins[0] || t < surface->texturemins[1])
            {
                continue;
            }

            s -= surface->texturemins[0];
            t -= surface->texturemins[1];

            if (s > surface->extents[0] || t > surface->extents[1])
            {
                continue;
            }

            return surface;
        }

        return TraceLineToSurface(model, node->children[!side], point, end);
    }
}

bool internalTraceLineToSky(model_t *_model, const Vector3 &start, const Vector3 &end)
{
    goldsrc::model_t *model = (goldsrc::model_t *)_model;
    const goldsrc::msurface_t *surface = TraceLineToSurface(model, model->nodes, start, end);
    if (!surface)
        return false;

    if (!(surface->flags & SURF_SKY))
        return false;

    return true;
}

static bool SampleLightmap(Vector3 &color, goldsrc::model_t *model, goldsrc::mnode_t *node, const Vector3 &start, const Vector3 &end)
{
    while (true)
    {
        if (node->contents < 0)
        {
            return false;
        }

        float dist1 = Dot(start, node->plane->normal) - node->plane->dist;
        float dist2 = Dot(end, node->plane->normal) - node->plane->dist;

        bool side = (dist1 < 0);
        if (side == (dist2 < 0))
        {
            node = node->children[side];
            continue;
        }

        float frac = dist1 / (dist1 - dist2);
        Vector3 point = VectorLerp(start, end, frac);

        if (SampleLightmap(color, model, node->children[side], start, point))
        {
            return true;
        }

        for (int i = 0; i < node->numsurfaces; i++)
        {
            const goldsrc::msurface_t *face = GetSurface(model, node->firstsurface + i);
            if (face->flags & SURF_SKY)
            {
                continue;
            }

            double ds = Dot(point, face->texinfo->vec_s) + face->texinfo->dist_s;
            double dt = Dot(point, face->texinfo->vec_t) + face->texinfo->dist_t;

            int s = (int)ds;
            int t = (int)dt;
            if (s < face->texturemins[0] || t < face->texturemins[1])
            {
                continue;
            }

            s -= face->texturemins[0];
            t -= face->texturemins[1];

            if (s > face->extents[0] || t > face->extents[1])
            {
                continue;
            }

            if (!face->samples)
            {
                return false;
            }

            int lightmap_width = (face->extents[0] / 16) + 1;
            int lightmap_height = (face->extents[1] / 16) + 1;
            int lightmap_x = s / 16;
            int lightmap_y = t / 16;

            color24 *lightmap = &face->samples[lightmap_y * lightmap_width + lightmap_x];

            for (int j = 0; j < MAXLIGHTMAPS; j++)
            {
                int style = face->styles[j];
                if (style >= MAX_LIGHTSTYLES)
                {
                    break;
                }

                float weight = g_lightstyles[style];
                color.x += lightmap->r * weight;
                color.y += lightmap->g * weight;
                color.z += lightmap->b * weight;

                lightmap += (lightmap_width * lightmap_height);
            }

            return true;
        }

        return SampleLightmap(color, model, node->children[!side], point, end);
    }
}

colorVec internalSampleLightmap(model_t *_model, const Vector3 &start, const Vector3 &end)
{
    goldsrc::model_t *model = (goldsrc::model_t *)_model;

    Vector3 colour{};
    SampleLightmap(colour, model, model->nodes, start, end);

    colorVec result{};
    result.r = Q_min((unsigned)colour.x, 255u);
    result.g = Q_min((unsigned)colour.y, 255u);
    result.b = Q_min((unsigned)colour.z, 255u);

    return result;
}

void internalUpdateViewmodelAnimation(cl_entity_t *viewmodel)
{
    // a 100% reliable way to get pointers to cl.weaponstarttime and cl.weaponsequence
    // this part of the client_state_t struct hasn't changed since sdk 2.0:
    struct Partial
    {
        cl_entity_t viewent;
        int cdtrack;
        int looptrack;
        CRC32_t serverCRC;
        unsigned char clientdllmd5[16];
        float weaponstarttime;
        int weaponsequence;
    };

    Partial *partial = (Partial *)g_engfuncs.GetViewModel();
    GL3_ASSERT(&partial->viewent == viewmodel);

    if (!partial->weaponstarttime)
    {
        partial->weaponstarttime = g_engfuncs.GetClientTime();
    }

    viewmodel->curstate.animtime = partial->weaponstarttime;
    viewmodel->curstate.sequence = partial->weaponsequence;
}

Color32 internalWaterColor(model_t *_model, int textureIndex)
{
    goldsrc::model_t *model = (goldsrc::model_t *)_model;

    if (textureIndex >= 0 && textureIndex < model->numtextures)
    {
        goldsrc::texture_t *texture = model->textures[textureIndex];
        GL3_ASSERT(texture);

        byte *palette = texture->pPal;
        byte r = palette[9];
        byte g = palette[10];
        byte b = palette[11];
        byte a = palette[12];

        return { r, g, b, a };
    }

    GL3_ASSERT(0);
    return {};
}

}
