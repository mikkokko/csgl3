#include "stdafx.h"
#include "decal.h"
#include "commandbuffer.h"
#include "dynamicbuffer.h"
#include "brush.h"
#include "internal.h"

namespace Render
{

// oversized on purpose
constexpr int MaxDecals = 512;
constexpr int MaxDecalIndices = MaxDecals * 6;

struct DrawnDecal
{
    GLuint texture;
    int indexOffset;
    int indexCount;
};

static cvar_t *gl_polyoffset;

static int s_decalCount;
static DrawnDecal s_decals[MaxDecals];

static DynamicVertexState s_vertexState;

static int s_indexCount;
static GLushort s_indexData[MaxDecalIndices];

void decalInit()
{
    gl_polyoffset = g_engfuncs.pfnGetCvarPointer("gl_polyoffset");
}

static void AddDecal(GLuint textureName, const gl3_brushvert_t *vertices, int vertexCount)
{
    GL3_ASSERT(vertexCount >= 3);

    // doing this on demand
    if (!s_vertexState.IsMapped())
    {
        s_vertexState.Map(sizeof(gl3_brushvert_t));
    }

    if (s_decalCount >= MaxDecals)
    {
        GL3_ASSERT(false);
        return;
    }

    int indexCount = (vertexCount - 2) * 3;
    if (s_indexCount + indexCount >= MaxDecalIndices)
    {
        GL3_ASSERT(false);
        return;
    }

    DrawnDecal *decal = &s_decals[s_decalCount++];
    decal->texture = textureName;
    decal->indexOffset = s_indexCount;
    decal->indexCount = indexCount;

    int indexBase = s_vertexState.IndexBase();
    for (int j = 1; j < vertexCount - 1; j++)
    {
        s_indexData[s_indexCount++] = static_cast<GLushort>(indexBase);
        s_indexData[s_indexCount++] = static_cast<GLushort>(indexBase + j);
        s_indexData[s_indexCount++] = static_cast<GLushort>(indexBase + j + 1);
    }

    // FIXME: check if this fits first....
    s_vertexState.WriteData(vertices, vertexCount);
}

void decalAddFromSurface(gl3_worldmodel_t *model, gl3_surface_t *surface)
{
    void *iterator = nullptr;

    while (true)
    {
        GLuint texture;
        int vertexCount;
        gl3_brushvert_t vertices[32];

        int surfaceIndex = surface - model->surfaces;
        iterator = internalSurfaceDecals(model->engine_model, iterator, surfaceIndex, &texture, vertices, &vertexCount);
        if (!iterator)
        {
            break;
        }

        // NOTE: internalSurfaceDecals only sets position and texcoords, we need to fill the rest
        for (int i = 0; i < vertexCount; i++)
        {
            vertices[i].position.w = (float)surface->lightmap_width / model->lightmap_width;

            vertices[i].texCoord.z = (vertices[i].texCoord.z + (surface->lightmap_x * 16) + 8) / (model->lightmap_width * 16);
            vertices[i].texCoord.w = (vertices[i].texCoord.w + (surface->lightmap_y * 16) + 8) / (model->lightmap_height * 16);

            for (int j = 0; j < MAXLIGHTMAPS; j++)
            {
                vertices[i].styles[j] = surface->styles[j];
            }
        }

        if (vertexCount != 0)
        {
            AddDecal(texture, vertices, vertexCount);
        }
    }
}

void decalDrawAll(DynamicIndexState &indexState)
{
    if (!s_decalCount)
    {
        return;
    }

    // fucked: we're drawing after brush model surfaces, and the index buffer for that
    // is still mapped so we can't map it here again... as a hacky workaround, write in
    // between of the brush indices (this can potentially overflow...)

    // FIXME: clean this up now that we support u16 indices for bmodels
    if (indexState.IndexSize() == 4)
    {
        indexState.WriteData(s_indexData, ((s_indexCount + 1) / 2));
    }
    else
    {
        GL3_ASSERT(indexState.IndexSize() == 2);
        indexState.WriteData(s_indexData, s_indexCount);
    }

    commandBindVertexBuffer(s_vertexState.VertexBuffer(), g_brushVertexFormat);

    commandBlendEnable(GL_TRUE);
    commandBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    commandDepthMask(GL_FALSE);
    commandPolygonOffset(-1.0f, -gl_polyoffset->value);

    // we're only interested about the offset
    int indexByteOffset;
    (void)indexState.UpdateDrawOffset(indexByteOffset);

    for (int i = 0; i < s_decalCount; i++)
    {
        const DrawnDecal *decal = &s_decals[i];
        commandBindTexture(0, GL_TEXTURE_2D, decal->texture);
        commandDrawElements(GL_TRIANGLES,
            decal->indexCount,
            GL_UNSIGNED_SHORT,
            indexByteOffset + (decal->indexOffset * sizeof(GLushort)));
    }

    // FIXME: this can break water!!!!
    commandPolygonOffset(0.0f, 0.0f);
    commandBlendEnable(GL_FALSE);
    commandDepthMask(GL_TRUE);

    s_decalCount = 0;
    s_indexCount = 0;

    s_vertexState.Unmap();
}

}
