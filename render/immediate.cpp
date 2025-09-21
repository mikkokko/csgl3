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
    Vector4 color;
};

struct ImmediateState
{
    bool active{ false };

    GLboolean blendEnable{ GL_FALSE };
    GLenum blendSrc{};
    GLenum blendDst{};

    GLboolean depthTest{ GL_TRUE };
    GLboolean depthMask{ GL_TRUE };

    GLboolean cullFace{ GL_TRUE };

    GLuint texture{};
};

static const VertexAttrib s_vertexAttribs[] = {
    VERTEX_ATTRIB(ImmediateVertex, position),
    VERTEX_ATTRIB(ImmediateVertex, texCoord),
    VERTEX_ATTRIB(ImmediateVertex, color),
    VERTEX_ATTRIB_TERM()
};

static const VertexFormat s_vertexFormat{ s_vertexAttribs, sizeof(ImmediateVertex) };

// shadowed state for batching
static ImmediateState s_state;

// Begin/End scope
static ImmediateVertex *s_vertexWrite;
static GLushort s_vertexWriteCount;
static GLenum s_primitive;
static Vector4 s_currentColor;
static Vector2 s_currentTexCoord;

static DynamicVertexState s_vertexState;
static DynamicIndexState s_indexState;

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

static void Flush()
{
    GL3_ASSERT(s_state.active);
    GL3_ASSERT(s_vertexWriteCount == 0);

    int indexByteOffset;
    int indexCount = s_indexState.UpdateDrawOffset(indexByteOffset);
    if (!indexCount)
    {
        // FIXME: if lines are ever supported, this won't work as they're not indexed
        return;
    }

    commandDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indexByteOffset);
}

void immediateInit()
{
    shaderRegister(s_shader);
    shaderRegister(s_shaderAlphaTest);
}

void immediateDrawStart(bool alphaTest)
{
    GL3_ASSERT(!s_state.active);
    s_state.active = true;

    GL3_ASSERT(s_vertexWriteCount == 0);

    s_vertexState.Map(sizeof(ImmediateVertex));
    s_indexState.Map(sizeof(GLushort));

    commandBindVertexBuffer(s_vertexState.VertexBuffer(), s_vertexFormat);
    commandBindIndexBuffer(s_indexState.IndexBuffer());

    SpriteShader &shader = alphaTest ? s_shaderAlphaTest : s_shader;
    commandUseProgram(&shader);
}

void immediateDrawEnd()
{
    Flush();

    GL3_ASSERT(s_vertexWriteCount == 0);

    // restore state... this is dumb but no other way currently
    {
        if (s_state.blendEnable)
        {
            commandBlendEnable(GL_FALSE);
        }

        if (!s_state.depthTest)
        {
            commandDepthTest(GL_TRUE);
        }

        if (!s_state.depthMask)
        {
            commandDepthMask(GL_TRUE);
        }

        if (!s_state.cullFace)
        {
            commandCullFace(GL_TRUE);
        }
    }

    GL3_ASSERT(s_state.active);
    s_state = {}; // FIXME: not necessarily needed

    s_vertexState.Unmap();
    s_indexState.Unmap();
}

bool immediateIsActive()
{
    return s_state.active;
}

void immediateBlendEnable(GLboolean enable)
{
    GL3_ASSERT(s_state.active);

    if (s_state.blendEnable != enable)
    {
        Flush();
        s_state.blendEnable = enable;
        commandBlendEnable(enable);
    }
}

void immediateBlendFunc(GLenum sfactor, GLenum dfactor)
{
    GL3_ASSERT(s_state.active);

    if (s_state.blendSrc != sfactor || s_state.blendDst != dfactor)
    {
        Flush();
        s_state.blendSrc = sfactor;
        s_state.blendDst = dfactor;
        commandBlendFunc(sfactor, dfactor);
    }
}

void immediateCullFace(GLboolean enable)
{
    GL3_ASSERT(s_state.active);

    if (s_state.cullFace != enable)
    {
        Flush();
        s_state.cullFace = enable;
        commandCullFace(enable);
    }
}

void immediateDepthTest(GLboolean enable)
{
    GL3_ASSERT(s_state.active);

    if (s_state.depthTest != enable)
    {
        Flush();
        s_state.depthTest = enable;
        commandDepthTest(enable);
    }
}

void immediateDepthMask(GLboolean flag)
{
    GL3_ASSERT(s_state.active);

    if (s_state.depthMask != flag)
    {
        Flush();
        s_state.depthMask = flag;
        commandDepthMask(flag);
    }
}

void immediateBindTexture(GLuint texture)
{
    GL3_ASSERT(s_state.active);

    if (s_state.texture != texture)
    {
        Flush();
        s_state.texture = texture;
        commandBindTexture(0, GL_TEXTURE_2D, texture);
    }
}

void immediateBegin(GLenum mode)
{
    GL3_ASSERT(s_state.active);

    GL3_ASSERT(mode == GL_TRIANGLES
        //|| mode == GL_TRIANGLE_FAN
        || mode == GL_QUADS
        //|| mode == GL_POLYGON
        //|| mode == GL_LINES
        //|| mode == GL_TRIANGLE_STRIP
        //|| mode == GL_QUAD_STRIP
    );

    s_primitive = mode;
    GL3_ASSERT(s_vertexWriteCount == 0);
    s_vertexWrite = static_cast<ImmediateVertex *>(s_vertexState.BeginWrite());
}

void immediateColor4f(float r, float g, float b, float a)
{
    GL3_ASSERT(s_state.active);

    s_currentColor.x = r;
    s_currentColor.y = g;
    s_currentColor.z = b;
    s_currentColor.w = a;
}

void immediateTexCoord2f(float s, float t)
{
    GL3_ASSERT(s_state.active);
    s_currentTexCoord.x = s;
    s_currentTexCoord.y = t;
}

void immediateVertex3f(float x, float y, float z)
{
    GL3_ASSERT(s_state.active);

    ImmediateVertex &vertex = s_vertexWrite[s_vertexWriteCount++];
    vertex.position.x = x;
    vertex.position.y = y;
    vertex.position.z = z;
    vertex.texCoord = s_currentTexCoord;
    vertex.color = s_currentColor;
}

void immediateEnd()
{
    GL3_ASSERT(s_state.active);

    GLushort indexBase = (GLushort)s_vertexState.IndexBase();

    int writeCount = 0;
    GLushort *indices = static_cast<GLushort *>(s_indexState.BeginWrite());

    switch (s_primitive)
    {
    case GL_TRIANGLES:
        for (GLushort i = 0; i < s_vertexWriteCount; i += 3)
        {
            indices[writeCount++] = indexBase + i;
            indices[writeCount++] = indexBase + i + 1;
            indices[writeCount++] = indexBase + i + 2;
        }
        break;

    case GL_QUADS:
        for (GLushort i = 0; i < s_vertexWriteCount; i += 4)
        {
            indices[writeCount++] = indexBase + i;
            indices[writeCount++] = indexBase + i + 1;
            indices[writeCount++] = indexBase + i + 2;
            indices[writeCount++] = indexBase + i;
            indices[writeCount++] = indexBase + i + 2;
            indices[writeCount++] = indexBase + i + 3;
        }
        break;

    default:
        GL3_ASSERT(false);
        break;
    }

    s_indexState.FinishWrite(writeCount);

    s_vertexState.FinishWrite(s_vertexWriteCount);
    s_vertexWriteCount = 0;
}

}
