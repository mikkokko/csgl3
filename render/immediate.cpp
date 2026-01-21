#include "stdafx.h"
#include "immediate.h"
#include "commandbuffer.h"
#include "dynamicbuffer.h"
#include "memory.h"

namespace Render
{

struct ImmediateVertex
{
    Vector3 position;
    Vector2 texCoord;
    uint8_t color[4];
    uint8_t padding[8];
};

static const VertexAttrib s_vertexAttribs[] = {
    VERTEX_ATTRIB(ImmediateVertex, position),
    VERTEX_ATTRIB(ImmediateVertex, texCoord),
    VERTEX_ATTRIB_NORM(ImmediateVertex, color),
    VERTEX_ATTRIB_TERM()
};

static const VertexFormat s_vertexFormat{ s_vertexAttribs, sizeof(ImmediateVertex) };

class SpriteShader : public BaseShader
{
public:
    const char *Name()
    {
        return "sprite";
    }

    const VertexAttrib *VertexAttribs()
    {
        return s_vertexAttribs;
    }

    const ShaderUniform *Uniforms()
    {
        static const ShaderUniform uniforms[] = {
            SHADER_UNIFORM_CONST(u_texture, 0),
            SHADER_UNIFORM_TERM()
        };

        return uniforms;
    }
};

class SpriteShaderAlphaTest : public SpriteShader
{
public:
    const char *Defines()
    {
        return "#define ALPHA_TEST 1\n";
    }
};

static SpriteShader s_shader;
static SpriteShaderAlphaTest s_shaderAlphaTest;

static bool s_active;

static BufferSpanT<ImmediateVertex> s_vertexSpan;
static BufferSpanT<uint16_t> s_indexSpan;
static int s_totalVertexCount;
static int s_totalIndexCount;
static int s_lastDrawIndexCount;
static GLenum s_currentMode;
static int s_primitiveVertexCount;
static int s_primitiveStartVertex;

// current vertex attributes
static Vector4 s_currentColor;
static Vector2 s_currentTexCoord;

static void Flush()
{
    int indexCount = s_totalIndexCount - s_lastDrawIndexCount;
    if (!indexCount)
    {
        return;
    }

    GL3_ASSERT(indexCount > 0);

    int indexOffset = s_indexSpan.byteOffset + s_lastDrawIndexCount * sizeof(uint16_t);
    int baseVertex = s_vertexSpan.byteOffset / sizeof(ImmediateVertex);

    commandDrawElementsBaseVertex(
        GL_TRIANGLES,
        indexCount,
        GL_UNSIGNED_SHORT,
        indexOffset,
        baseVertex);

    s_lastDrawIndexCount = s_totalIndexCount;
}

void immediateInit()
{
    shaderRegister(s_shader);
    shaderRegister(s_shaderAlphaTest);
}

// FIXME: isn't this too small? our dynamic vertex buffers are tiny
constexpr int MaxVertexCount = 8192;

// all quads is the worst case for now
constexpr int MaxIndexCount = (MaxVertexCount * 3) / 2;

void immediateDrawStart(bool alphaTest)
{
    GL3_ASSERT(!s_active);
    s_active = true;

    s_vertexSpan = dynamicVertexDataBegin<ImmediateVertex>(MaxVertexCount);
    s_indexSpan = dynamicIndexDataBegin<uint16_t>(MaxIndexCount);
    s_totalVertexCount = 0;
    s_totalIndexCount = 0;
    s_lastDrawIndexCount = 0;

    commandBindVertexBuffer(s_vertexSpan.buffer, s_vertexFormat);
    commandBindIndexBuffer(s_indexSpan.buffer);

    SpriteShader &shader = alphaTest ? s_shaderAlphaTest : s_shader;
    commandUseProgram(&shader);
}

void immediateDrawEnd()
{
    GL3_ASSERT(s_active);

    Flush();

    dynamicVertexDataEnd<ImmediateVertex>(s_totalVertexCount);
    dynamicIndexDataEnd<uint16_t>(s_totalIndexCount);

    // restore state... this is dumb but no other way currently
    commandBlendEnable(GL_FALSE);
    commandDepthTest(GL_TRUE);
    commandDepthMask(GL_TRUE);
    commandCullFace(GL_TRUE);

    s_active = false;
}

bool immediateIsActive()
{
    return s_active;
}

void immediateBlendEnable(GLboolean enable)
{
    GL3_ASSERT(s_active);

    if (g_shadowState.blendEnable != enable)
    {
        Flush();
        commandBlendEnable(enable);
    }
}

void immediateBlendFunc(GLenum sfactor, GLenum dfactor)
{
    GL3_ASSERT(s_active);

    if (g_shadowState.blendSrc != sfactor || g_shadowState.blendDst != dfactor)
    {
        Flush();
        commandBlendFunc(sfactor, dfactor);
    }
}

void immediateCullFace(GLboolean enable)
{
    GL3_ASSERT(s_active);

    if (g_shadowState.cullFace != enable)
    {
        Flush();
        commandCullFace(enable);
    }
}

void immediateDepthTest(GLboolean enable)
{
    GL3_ASSERT(s_active);

    if (g_shadowState.depthTest != enable)
    {
        Flush();
        commandDepthTest(enable);
    }
}

void immediateDepthMask(GLboolean flag)
{
    GL3_ASSERT(s_active);

    if (g_shadowState.depthMask != flag)
    {
        Flush();
        commandDepthMask(flag);
    }
}

void immediateBindTexture(GLuint texture)
{
    GL3_ASSERT(s_active);

    if (g_shadowState.texture2Ds[0] != texture)
    {
        Flush();
        commandBindTexture(0, GL_TEXTURE_2D, texture);
    }
}

void immediateBegin(GLenum mode)
{
    GL3_ASSERT(s_active);

    s_currentMode = mode;
    s_primitiveVertexCount = 0;
    s_primitiveStartVertex = s_totalVertexCount;
}

void immediateColor4f(float r, float g, float b, float a)
{
    GL3_ASSERT(s_active);
    s_currentColor = { r, g, b, a };
}

void immediateTexCoord2f(float s, float t)
{
    GL3_ASSERT(s_active);
    s_currentTexCoord = { s, t };
}

void immediateVertex3f(float x, float y, float z)
{
    GL3_ASSERT(s_active);

    ImmediateVertex *v = &s_vertexSpan.data[s_totalVertexCount];
    v->position = { x, y, z };
    v->texCoord = s_currentTexCoord;
    v->color[0] = static_cast<uint8_t>(s_currentColor.x * 255.0f);
    v->color[1] = static_cast<uint8_t>(s_currentColor.y * 255.0f);
    v->color[2] = static_cast<uint8_t>(s_currentColor.z * 255.0f);
    v->color[3] = static_cast<uint8_t>(s_currentColor.w * 255.0f);

    s_totalVertexCount++;
    s_primitiveVertexCount++;
}

void immediateEnd()
{
    GL3_ASSERT(s_active);

    if (s_currentMode == GL_TRIANGLES)
    {
        int triangleCount = s_primitiveVertexCount / 3;
        for (int i = 0; i < triangleCount; i++)
        {
            uint16_t base = static_cast<uint16_t>(s_primitiveStartVertex + i * 3);
            s_indexSpan.data[s_totalIndexCount++] = base + 0;
            s_indexSpan.data[s_totalIndexCount++] = base + 1;
            s_indexSpan.data[s_totalIndexCount++] = base + 2;
        }
    }
    else if (s_currentMode == GL_QUADS)
    {
        int quadCount = s_primitiveVertexCount / 4;
        for (int i = 0; i < quadCount; i++)
        {
            uint16_t base = static_cast<uint16_t>(s_primitiveStartVertex + i * 4);
            s_indexSpan.data[s_totalIndexCount++] = base + 0;
            s_indexSpan.data[s_totalIndexCount++] = base + 1;
            s_indexSpan.data[s_totalIndexCount++] = base + 2;
            s_indexSpan.data[s_totalIndexCount++] = base + 0;
            s_indexSpan.data[s_totalIndexCount++] = base + 2;
            s_indexSpan.data[s_totalIndexCount++] = base + 3;
        }
    }
    else
    {
        GL3_ASSERT(false);
    }
}

}
