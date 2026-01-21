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

// rough approximation, gets checked so we won't overflow
constexpr int MaxDecalVertices = MaxDecals * 4;

struct DrawnDecal
{
    GLuint texture;
    int vertexOffset;
    int vertexCount;
};

static cvar_t *gl_polyoffset;

static int s_decalCount;
static DrawnDecal s_decals[MaxDecals];

static int s_vertexCount;
static BufferSpanT<gl3_brushvert_t> s_vertexSpan;

void decalInit()
{
    gl_polyoffset = g_engfuncs.pfnGetCvarPointer("gl_polyoffset");
}

void decalAdd(GLuint textureName, const gl3_brushvert_t *vertices, int vertexCount)
{
    GL3_ASSERT(vertexCount >= 3);

    if (s_decalCount >= MaxDecals)
    {
        GL3_ASSERT(false);
        return;
    }

    if (s_vertexCount + vertexCount > MaxDecalVertices)
    {
        GL3_ASSERT(false);
        return;
    }

    DrawnDecal *decal = &s_decals[s_decalCount++];
    decal->texture = textureName;
    decal->vertexOffset = s_vertexCount;
    decal->vertexCount = vertexCount;

    // lock on demand
    if (!s_vertexSpan.buffer)
    {
        s_vertexSpan = dynamicVertexDataBegin<gl3_brushvert_t>(MaxDecalVertices);
    }

    memcpy(&s_vertexSpan.data[s_vertexCount], vertices, vertexCount * sizeof(gl3_brushvert_t));
    s_vertexCount += vertexCount;
}

int decalDrawAll(uint16_t *spanData, int spanOffsetBytes, int curIndexCount)
{
    if (!s_decalCount)
    {
        return curIndexCount;
    }

    GL3_ASSERT((s_vertexSpan.byteOffset % sizeof(gl3_brushvert_t)) == 0);
    int baseVertex = s_vertexSpan.byteOffset / sizeof(gl3_brushvert_t);

    dynamicVertexDataEnd<gl3_brushvert_t>(s_vertexCount);
    s_vertexCount = 0;

    commandBindVertexBuffer(s_vertexSpan.buffer, g_brushVertexFormat);
    s_vertexSpan.buffer = 0;

    commandBlendEnable(GL_TRUE);
    commandBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    commandDepthMask(GL_FALSE);
    commandPolygonOffset(-1.0f, -gl_polyoffset->value);

    for (int i = 0; i < s_decalCount; i++)
    {
        const DrawnDecal *decal = &s_decals[i];

        const int indexCount = (decal->vertexCount - 2) * 3;
        const int indexByteOffset = spanOffsetBytes + (curIndexCount * sizeof(uint16_t));

        const int vertexCount = decal->vertexCount;
        const int vertexOffset = decal->vertexOffset;

        for (int j = 1; j < vertexCount - 1; j++)
        {
            spanData[curIndexCount++] = static_cast<GLushort>(vertexOffset);
            spanData[curIndexCount++] = static_cast<GLushort>(vertexOffset + j);
            spanData[curIndexCount++] = static_cast<GLushort>(vertexOffset + j + 1);
        }

        commandBindTexture(0, GL_TEXTURE_2D, decal->texture);
        commandDrawElementsBaseVertex(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indexByteOffset, baseVertex);
    }

    // FIXME: this can break water!!!!
    commandPolygonOffset(0.0f, 0.0f);
    commandBlendEnable(GL_FALSE);
    commandDepthMask(GL_TRUE);

    s_decalCount = 0;

    return curIndexCount;
}

}
