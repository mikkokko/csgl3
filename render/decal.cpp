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

struct DrawnDecal
{
    GLuint texture;
    int indexBase;
    int vertexCount;
};

static cvar_t *gl_polyoffset;

static int s_decalCount;
static DrawnDecal s_decals[MaxDecals];

void decalInit()
{
    gl_polyoffset = g_engfuncs.pfnGetCvarPointer("gl_polyoffset");
}

static void AddDecal(GLuint textureName, const gl3_brushvert_t *vertices, int vertexCount)
{
    GL3_ASSERT(vertexCount >= 3);

    if (s_decalCount >= MaxDecals)
    {
        GL3_ASSERT(false);
        return;
    }

    // doing this on demand
    if (!g_dynamicVertexState.IsLocked())
    {
        g_dynamicVertexState.Lock(sizeof(gl3_brushvert_t));
    }

    DrawnDecal *decal = &s_decals[s_decalCount++];
    decal->texture = textureName;
    decal->indexBase = g_dynamicVertexState.IndexBase();
    decal->vertexCount = vertexCount;

    // FIXME: check if this fits first....
    g_dynamicVertexState.Write(vertices, vertexCount);
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

    // done writing vertices at this point
    g_dynamicVertexState.Unlock();

    commandBindVertexBuffer(g_dynamicVertexState.VertexBuffer(), g_brushVertexFormat);

    commandBlendEnable(GL_TRUE);
    commandBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    commandDepthMask(GL_FALSE);
    commandPolygonOffset(-1.0f, -gl_polyoffset->value);

    for (int i = 0; i < s_decalCount; i++)
    {
        const DrawnDecal *decal = &s_decals[i];

        int indexCount = 0;
        int indexBase = decal->indexBase;
        GLushort *indices = static_cast<GLushort *>(indexState.BeginWrite());

        for (int j = 1; j < decal->vertexCount - 1; j++)
        {
            indices[indexCount++] = static_cast<GLushort>(indexBase);
            indices[indexCount++] = static_cast<GLushort>(indexBase + j);
            indices[indexCount++] = static_cast<GLushort>(indexBase + j + 1);
        }

        // index state might have 32-bit indices so we need to do this
        if (indexState.IndexSize() == 4)
        {
            indexState.FinishWrite((indexCount + 1) / 2);
        }
        else
        {
            GL3_ASSERT(indexState.IndexSize() == 2);
            indexState.FinishWrite(indexCount);
        }

        int indexByteOffset;
        indexState.UpdateDrawOffset(indexByteOffset);

        commandBindTexture(0, GL_TEXTURE_2D, decal->texture);
        commandDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indexByteOffset);
    }

    // FIXME: this can break water!!!!
    commandPolygonOffset(0.0f, 0.0f);
    commandBlendEnable(GL_FALSE);
    commandDepthMask(GL_TRUE);

    s_decalCount = 0;
}

}
