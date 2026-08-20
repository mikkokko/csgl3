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
};

static const VertexAttrib s_vertexAttribs[] = {
    { &ImmediateVertex::position, "a_position" },
    { &ImmediateVertex::texCoord, "a_texCoord" },
    { &ImmediateVertex::color, "a_color", true },
};

static const VertexFormat s_vertexFormat{ sizeof(ImmediateVertex), s_vertexAttribs };

static const ShaderUniform s_uniforms[] = {
    { "u_texture", 0 }
};

static constexpr ShaderOption s_shaderOptions[] = {
    { "ALPHA_TEST", 1 }
};

// must match s_shaderOptions
struct SpriteShaderOptions
{
    unsigned alphaTest;
};

static BaseShader s_shaders[shaderVariantCount(s_shaderOptions)];

struct Primitive
{
    uint16_t mode; // GLenum
    uint16_t vertexCount;
};

// 16 bit indices, so this should be 65536, but have it at 65535
// so we can have Primitive::vertexCount as an uint16 without overflow woes
constexpr int MaxStageVertices = UINT16_MAX;

// worst case is all triangles
constexpr int MaxStagePrimitives = MaxStageVertices / 3;

static bool s_active;

static ImmediateVertex *s_stageVertices;
static int s_stageVertexCount;

static Primitive *s_stagePrimitives;
static int s_stagePrimitiveCount;

// number of indices the staged primitives are going to generate
static int s_stageIndexCount;

static GLenum s_currentMode;
static int s_primitiveStartVertex;

// current vertex attributes
static ImmediateVertex s_currentVertex;

static uint16_t *EmitTRIANGLES(uint16_t *dst, int first, int vertexCount)
{
    for (int i = 0; i + 3 <= vertexCount; i += 3)
    {
        *dst++ = static_cast<uint16_t>(first + i + 0);
        *dst++ = static_cast<uint16_t>(first + i + 1);
        *dst++ = static_cast<uint16_t>(first + i + 2);
    }

    return dst;
}

static uint16_t *EmitQUADS(uint16_t *dst, int first, int vertexCount)
{
    for (int i = 0; i + 4 <= vertexCount; i += 4)
    {
        *dst++ = static_cast<uint16_t>(first + i + 0);
        *dst++ = static_cast<uint16_t>(first + i + 1);
        *dst++ = static_cast<uint16_t>(first + i + 2);
        *dst++ = static_cast<uint16_t>(first + i + 0);
        *dst++ = static_cast<uint16_t>(first + i + 2);
        *dst++ = static_cast<uint16_t>(first + i + 3);
    }

    return dst;
}

static uint16_t *EmitQUAD_STRIP(uint16_t *dst, int first, int vertexCount)
{
    for (int i = 2; i + 1 < vertexCount; i += 2)
    {
        *dst++ = static_cast<uint16_t>(first + i - 2);
        *dst++ = static_cast<uint16_t>(first + i - 1);
        *dst++ = static_cast<uint16_t>(first + i + 1);
        *dst++ = static_cast<uint16_t>(first + i - 2);
        *dst++ = static_cast<uint16_t>(first + i + 1);
        *dst++ = static_cast<uint16_t>(first + i + 0);
    }

    return dst;
}

static void EmitIndices(uint16_t *spanData, int indexBias)
{
    uint16_t *dst = spanData;
    int first = indexBias;

    for (int i = 0; i < s_stagePrimitiveCount; i++)
    {
        const Primitive &primitive = s_stagePrimitives[i];
        int primitiveVertexCount = primitive.vertexCount;

        switch (primitive.mode)
        {
        case GL_TRIANGLES:
            dst = EmitTRIANGLES(dst, first, primitiveVertexCount);
            break;

        case GL_QUADS:
            dst = EmitQUADS(dst, first, primitiveVertexCount);
            break;

        case GL_QUAD_STRIP:
            dst = EmitQUAD_STRIP(dst, first, primitiveVertexCount);
            break;

        default:
            // already encountered by PrimitiveIndexCount
            GL3_ASSERT(false);
            break;
        }

        first += primitiveVertexCount;
    }

    GL3_ASSERT(dst - spanData == s_stageIndexCount);
}

static int PrimitiveIndexCount(GLenum mode, int vertexCount)
{
    switch (mode)
    {
    case GL_TRIANGLES:
        return (vertexCount / 3) * 3;

    case GL_QUADS:
        return (vertexCount / 4) * 6;

    case GL_QUAD_STRIP:
        return (vertexCount >= 4) ? ((vertexCount - 2) / 2) * 6 : 0;
    }

    // don't sweep these under the rug
    platformError("Unsupported primitive type 0x%04x", mode);
}

static void Flush()
{
    if (!s_stageIndexCount)
    {
        // shouldn't have to wipe these but just in case
        s_stageVertexCount = 0;
        s_stagePrimitiveCount = 0;
        return;
    }

    int vertexCount = s_stageVertexCount;

    auto vertexSpan = dynamicVertexDataBegin<ImmediateVertex>(vertexCount);
    memcpy(vertexSpan.data, s_stageVertices, vertexCount * sizeof(ImmediateVertex));
    dynamicVertexDataEnd<ImmediateVertex>(vertexCount);

    GL3_ASSERT((vertexSpan.byteOffset % sizeof(ImmediateVertex)) == 0);
    int baseVertex = vertexSpan.byteOffset / sizeof(ImmediateVertex);

    // avoid using a non zero base vertex by any means necessary
    int indexBias = 0;
    if (baseVertex + vertexCount <= MaxStageVertices)
    {
        indexBias = baseVertex;
        baseVertex = 0;
    }

    auto indexSpan = dynamicIndexDataBegin<uint16_t>(s_stageIndexCount);
    EmitIndices(indexSpan.data, indexBias);
    dynamicIndexDataEnd<uint16_t>(s_stageIndexCount);

    commandBindVertexBuffer(vertexSpan.buffer, s_vertexFormat, baseVertex);

    commandBindIndexBuffer(indexSpan.buffer);

    commandDrawElements(
        GL_TRIANGLES,
        s_stageIndexCount,
        GL_UNSIGNED_SHORT,
        indexSpan.byteOffset);

    s_stageVertexCount = 0;
    s_stagePrimitiveCount = 0;
    s_stageIndexCount = 0;
}

void immediateInit()
{
    s_stageVertices = memoryStaticAlloc<ImmediateVertex>(MaxStageVertices);
    s_stagePrimitives = memoryStaticAlloc<Primitive>(MaxStagePrimitives);
    shaderRegister(s_shaders, "sprite", s_vertexAttribs, s_uniforms, s_shaderOptions);
}

void immediateDrawStart(bool alphaTest)
{
    GL3_ASSERT(!s_active);
    s_active = true;

    SpriteShaderOptions options{};
    options.alphaTest = alphaTest ? 1 : 0;
    BaseShader &shader = shaderSelect(s_shaders, s_shaderOptions, options);
    commandUseProgram(&shader);
}

void immediateDrawEnd()
{
    GL3_ASSERT(s_active);

    Flush();

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
    s_primitiveStartVertex = s_stageVertexCount;
}

void immediateColor4f(float r, float g, float b, float a)
{
    GL3_ASSERT(s_active);

    s_currentVertex.color[0] = static_cast<uint8_t>(Q_clamp(r * 255.0f, 0.0f, 255.0f));
    s_currentVertex.color[1] = static_cast<uint8_t>(Q_clamp(g * 255.0f, 0.0f, 255.0f));
    s_currentVertex.color[2] = static_cast<uint8_t>(Q_clamp(b * 255.0f, 0.0f, 255.0f));
    s_currentVertex.color[3] = static_cast<uint8_t>(Q_clamp(a * 255.0f, 0.0f, 255.0f));
}

void immediateTexCoord2f(float s, float t)
{
    GL3_ASSERT(s_active);

    s_currentVertex.texCoord = { s, t };
}

void immediateVertex3f(float x, float y, float z)
{
    GL3_ASSERT(s_active);

    if (s_stageVertexCount >= MaxStageVertices)
    {
        platformError("Immediate mode vertex overflow");
    }

    s_currentVertex.position = { x, y, z };
    s_stageVertices[s_stageVertexCount++] = s_currentVertex;
}

void immediateEnd()
{
    GL3_ASSERT(s_active);

    GL3_ASSERT(s_stageVertexCount >= s_primitiveStartVertex);
    int vertexCount = s_stageVertexCount - s_primitiveStartVertex;

    int indexCount = PrimitiveIndexCount(s_currentMode, vertexCount);
    if (!indexCount)
    {
        // shouldn't happen
        s_stageVertexCount = s_primitiveStartVertex;
        return;
    }

    GL3_ASSERT(vertexCount <= UINT16_MAX);
    GL3_ASSERT(s_stagePrimitiveCount < MaxStagePrimitives);

    s_stagePrimitives[s_stagePrimitiveCount++] = {
        static_cast<uint16_t>(s_currentMode),
        static_cast<uint16_t>(vertexCount)
    };

    s_stageIndexCount += indexCount;
}

}
