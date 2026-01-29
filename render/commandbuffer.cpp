#include "stdafx.h"
#include "commandbuffer.h"

namespace Render
{

// should be enough for anything except the torture maps
constexpr int InitialBufferCapacity = 16834;

enum Command
{
    CmdActiveTexture,

    CmdBindUniformBuffer0,
    CmdBindUniformBuffer1,
    CmdBindUniformBuffer2,

    CmdBindTexture2D,
    CmdBindTextureCubeMap,
    CmdBlendFunc,
    CmdDepthFunc,
    CmdDepthMask,

    CmdDrawElementsBaseVertex,

    CmdPolygonOffset,
    CmdUniform1f,
    CmdUniform1i,
    CmdUseProgram,
    CmdBindVertexBuffer,
    CmdBindIndexBuffer,

    CmdBlendEnable,
    CmdCullFaceEnable,
    CmdDepthTestEnable,

    CmdBlendDisable,
    CmdCullFaceDisable,
    CmdDepthTestDisable,

    CmdCount
};

ShadowState g_shadowState;

static bool s_recording;

static size_t s_readOffset;
static size_t s_size;
static size_t s_capacity;
static uint32_t *s_buffer;

void commandInit()
{
    s_capacity = InitialBufferCapacity;
    s_buffer = static_cast<uint32_t *>(malloc(s_capacity * sizeof(uint32_t)));
    if (!s_buffer)
    {
        platformError("Command buffer allocation failed");
    }
}

static void Resize()
{
    GL3_ASSERT(s_size == s_capacity);
    s_capacity *= 2;
    s_buffer = static_cast<uint32_t *>(realloc(s_buffer, s_capacity * sizeof(uint32_t)));
    if (!s_buffer)
    {
        platformError("Command buffer reallocation failed");
    }
}

void commandRecord()
{
    GL3_ASSERT(!s_recording);
    s_recording = true;

    GL3_ASSERT(!s_size);
    GL3_ASSERT(s_readOffset == 0);

#ifdef SCHIZO_DEBUG
    g_state.drawcallCount = 0;
#endif

    // state reset
    g_shadowState = ShadowState{};
}

template<class To, class From>
static To BitCast(From value)
{
    union
    {
        From a;
        To b;
    } covert{ value };

    return covert.b;
}

template<typename T>
static void WriteWord(const T &value)
{
    GL3_ASSERT(s_size <= s_capacity);
    if (s_size == s_capacity)
    {
        Resize();
    }

    static_assert(sizeof(T) == 4, "bruh");
    uint32_t value32 = BitCast<uint32_t>(value);
    s_buffer[s_size++] = value32;
}

template<typename T>
static T ReadWord()
{
    GL3_ASSERT(s_readOffset < s_size);

    static_assert(sizeof(T) == 4, "bruh");
    uint32_t value32 = s_buffer[s_readOffset++];
    return BitCast<T>(value32);
}

static bool IsFinished()
{
    GL3_ASSERT(s_readOffset <= s_size);
    return s_readOffset == s_size;
}

void commandExecute()
{
    GL3_ASSERT(s_recording);
    s_recording = false;

    GL3_ASSERT(s_size);
    GL3_ASSERT(s_readOffset == 0);

    while (!IsFinished())
    {
        GL_ERRORS();
        Command cmd = ReadWord<Command>();
        switch (cmd)
        {
        case CmdActiveTexture:
        {
            GLuint unit = ReadWord<GLuint>();
            GLenum texture = GL_TEXTURE0 + unit;
            glActiveTexture(texture);
        }
        break;

        case CmdBindUniformBuffer0:
        {
            //GLenum target = ReadWord<GLenum>();
            //GLuint index = ReadWord<GLuint>();
            GLuint buffer = ReadWord<GLuint>();
            GLintptr offset = ReadWord<GLintptr>();
            GLsizeiptr size = ReadWord<GLsizeiptr>();
            glBindBufferRange(GL_UNIFORM_BUFFER, 0, buffer, offset, size);
        }
        break;

        case CmdBindUniformBuffer1:
        {
            //GLenum target = ReadWord<GLenum>();
            //GLuint index = ReadWord<GLuint>();
            GLuint buffer = ReadWord<GLuint>();
            GLintptr offset = ReadWord<GLintptr>();
            GLsizeiptr size = ReadWord<GLsizeiptr>();
            glBindBufferRange(GL_UNIFORM_BUFFER, 1, buffer, offset, size);
        }
        break;

        case CmdBindUniformBuffer2:
        {
            //GLenum target = ReadWord<GLenum>();
            //GLuint index = ReadWord<GLuint>();
            GLuint buffer = ReadWord<GLuint>();
            GLintptr offset = ReadWord<GLintptr>();
            GLsizeiptr size = ReadWord<GLsizeiptr>();
            glBindBufferRange(GL_UNIFORM_BUFFER, 2, buffer, offset, size);
        }
        break;

        case CmdBindTexture2D:
        {
            GLuint texture = ReadWord<GLuint>();
            glBindTexture(GL_TEXTURE_2D, texture);
        }
        break;

        case CmdBindTextureCubeMap:
        {
            GLuint texture = ReadWord<GLuint>();
            glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        }
        break;

        case CmdBlendFunc:
        {
            GLenum sfactor = ReadWord<GLenum>();
            GLenum dfactor = ReadWord<GLenum>();
            glBlendFunc(sfactor, dfactor);
        }
        break;

        case CmdDepthFunc:
        {
            GLenum func = ReadWord<GLenum>();
            glDepthFunc(func);
        }
        break;

        case CmdDepthMask:
        {
            GLint flag = ReadWord<GLint>();
            glDepthMask((GLboolean)flag);
        }
        break;

        case CmdDrawElementsBaseVertex:
        {
            //GLenum mode = ReadWord<GLenum>();
            GLsizei count = ReadWord<GLsizei>();
            //GLenum type = ReadWord<GLenum>();
            GLsizei offset = ReadWord<GLsizei>();
            GLint basevertex = ReadWord<GLint>();
            glDrawElementsBaseVertex(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(offset), basevertex);
#ifdef SCHIZO_DEBUG
            g_state.drawcallCount++;
#endif
        }
        break;

        case CmdPolygonOffset:
        {
            GLfloat factor = ReadWord<GLfloat>();
            GLfloat units = ReadWord<GLfloat>();
            if (factor == 0.0f && units == 0.0f)
            {
                glDisable(GL_POLYGON_OFFSET_FILL);
            }
            else
            {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(factor, units);
            }
        }
        break;

        case CmdUniform1f:
        {
            GLint location = ReadWord<GLint>();
            GLfloat v0 = ReadWord<GLfloat>();
            glUniform1f(location, v0);
        }
        break;

        case CmdUniform1i:
        {
            GLint location = ReadWord<GLint>();
            GLint v0 = ReadWord<GLint>();
            glUniform1i(location, v0);
        }
        break;

        case CmdUseProgram:
        {
            GLuint program = ReadWord<GLuint>();
            glUseProgram(program);
        }
        break;

        case CmdBindVertexBuffer:
        {
            int i;

            GLuint buffer = ReadWord<GLuint>();
            const VertexFormat *format = ReadWord<const VertexFormat *>();
            Span<const VertexAttrib> vertexAttribs = format->attribs;
            int vertexStride = format->stride;

            glBindBuffer(GL_ARRAY_BUFFER, buffer);

            for (i = 0; i < vertexAttribs.size(); i++)
            {
                const VertexAttrib &attrib = vertexAttribs[i];

                glEnableVertexAttribArray(i);
                glVertexAttribPointer(i, attrib.size, attrib.type, attrib.normalized, vertexStride, reinterpret_cast<void *>(static_cast<intptr_t>(attrib.offset)));
            }

            GL3_ASSERT(i <= MaxVertexAttribs);

            for (; i < MaxVertexAttribs; i++)
            {
                glDisableVertexAttribArray(i);
            }
        }
        break;

        case CmdBindIndexBuffer:
        {
            GLuint buffer = ReadWord<GLuint>();
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
        }
        break;

        case CmdBlendEnable:
        {
            glEnable(GL_BLEND);
        }
        break;

        case CmdCullFaceEnable:
        {
            glEnable(GL_CULL_FACE);
        }
        break;

        case CmdDepthTestEnable:
        {
            glEnable(GL_DEPTH_TEST);
        }
        break;

        case CmdBlendDisable:
        {
            glDisable(GL_BLEND);
        }
        break;

        case CmdCullFaceDisable:
        {
            glDisable(GL_CULL_FACE);
        }
        break;

        case CmdDepthTestDisable:
        {
            glDisable(GL_DEPTH_TEST);
        }
        break;

        default:
        {
            GL3_ASSERT(false);
        }
        break;
        }

        GL_ERRORS();
    }

#ifdef SCHIZO_DEBUG
    g_state.commandBufferSize = s_size;
#endif

    s_size = 0;
    s_readOffset = 0;
}

void commandBindUniformBuffer(GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    GL3_ASSERT(s_recording);
    GL3_ASSERT(index == 0 || index == 1 || index == 2);

    WriteWord(CmdBindUniformBuffer0 + index);
    //WriteWord(index);
    WriteWord(buffer);
    WriteWord(offset);
    WriteWord(size);
}

void commandBindTexture(GLuint unit, GLenum target, GLuint texture)
{
    GL3_ASSERT(s_recording);
    GL3_ASSERT(unit < MaxTextureUnits);

    if (g_shadowState.textureUnit != unit)
    {
        g_shadowState.textureUnit = unit;
        WriteWord(CmdActiveTexture);
        WriteWord(unit);
    }

    switch (target)
    {
    case GL_TEXTURE_2D:
        if (g_shadowState.texture2Ds[unit] != texture)
        {
            g_shadowState.texture2Ds[unit] = texture;
            WriteWord(CmdBindTexture2D);
            WriteWord(texture);
        }
        break;

    case GL_TEXTURE_CUBE_MAP:
        if (g_shadowState.textureCubeMaps[unit] != texture)
        {
            g_shadowState.textureCubeMaps[unit] = texture;
            WriteWord(CmdBindTextureCubeMap);
            WriteWord(texture);
        }
        break;

    default:
        GL3_ASSERT(false);
        break;
    }
}

void commandBlendFunc(GLenum sfactor, GLenum dfactor)
{
    GL3_ASSERT(s_recording);

    if (g_shadowState.blendSrc != sfactor || g_shadowState.blendDst != dfactor)
    {
        g_shadowState.blendSrc = sfactor;
        g_shadowState.blendDst = dfactor;
        WriteWord(CmdBlendFunc);
        WriteWord(sfactor);
        WriteWord(dfactor);
    }
}

void commandDepthFunc(GLenum func)
{
    GL3_ASSERT(s_recording);

    if (g_shadowState.depthFunc != func)
    {
        g_shadowState.depthFunc = func;
        WriteWord(CmdDepthFunc);
        WriteWord(func);
    }
}

void commandDepthMask(GLboolean flag)
{
    GL3_ASSERT(s_recording);

    if (g_shadowState.depthMask != flag)
    {
        g_shadowState.depthMask = flag;
        WriteWord(CmdDepthMask);
        WriteWord((GLint)flag);
    }
}

void commandBlendEnable(GLboolean enable)
{
    GL3_ASSERT(s_recording);

    if (g_shadowState.blendEnable != enable)
    {
        g_shadowState.blendEnable = enable;
        WriteWord(enable ? CmdBlendEnable : CmdBlendDisable);
    }
}

void commandCullFace(GLboolean enable)
{
    GL3_ASSERT(s_recording);

    if (g_shadowState.cullFace != enable)
    {
        g_shadowState.cullFace = enable;
        WriteWord(enable ? CmdCullFaceEnable : CmdCullFaceDisable);
    }
}

void commandDepthTest(GLboolean enable)
{
    GL3_ASSERT(s_recording);

    if (g_shadowState.depthTest != enable)
    {
        g_shadowState.depthTest = enable;
        WriteWord(enable ? CmdDepthTestEnable : CmdDepthTestDisable);
    }
}

void commandDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, GLsizei offset, GLint basevertex)
{
    GL3_ASSERT(s_recording);
    GL3_ASSERT(mode == GL_TRIANGLES);
    GL3_ASSERT(type == GL_UNSIGNED_SHORT);
    GL3_ASSERT(basevertex >= 0);

    WriteWord(CmdDrawElementsBaseVertex);
    //WriteWord(mode);
    WriteWord(count);
    //WriteWord(type);
    WriteWord(offset);
    WriteWord(basevertex);
}

void commandPolygonOffset(GLfloat factor, GLfloat units)
{
    GL3_ASSERT(s_recording);

    WriteWord(CmdPolygonOffset);
    WriteWord(factor);
    WriteWord(units);
}

void commandUniform1f(GLint location, GLfloat v0)
{
    GL3_ASSERT(s_recording);
    GL3_ASSERT(g_shadowState.shader);

    if (location == -1)
    {
        GL3_ASSERT(false); // wtf
        return;
    }

    UniformValue &value = g_shadowState.shader->uniformState[location];
    if (value.float_ != v0)
    {
        value.float_ = v0;
        WriteWord(CmdUniform1f);
        WriteWord(location);
        WriteWord(v0);
    }
}

void commandUniform1i(GLint location, GLint v0)
{
    GL3_ASSERT(s_recording);
    GL3_ASSERT(g_shadowState.shader);

    if (location == -1)
    {
        //GL3_ASSERT(false);
        return;
    }

    UniformValue &value = g_shadowState.shader->uniformState[location];
    if (value.int_ != v0)
    {
        value.int_ = v0;
        WriteWord(CmdUniform1i);
        WriteWord(location);
        WriteWord(v0);
    }
}

void commandUseProgram(BaseShader *shader)
{
    GL3_ASSERT(s_recording);

    if (g_shadowState.shader != shader)
    {
        g_shadowState.shader = shader;
        WriteWord(CmdUseProgram);
        WriteWord(shader->program);
    }
}

void commandBindIndexBuffer(GLuint buffer)
{
    if (g_shadowState.indexBuffer != buffer)
    {
        g_shadowState.indexBuffer = buffer;
        WriteWord(CmdBindIndexBuffer);
        WriteWord(buffer);
    }
}

void commandBindVertexBuffer(GLuint buffer, const VertexFormat &format)
{
    if (g_shadowState.vertexBuffer != buffer || g_shadowState.vertexFormat != &format)
    {
        g_shadowState.vertexBuffer = buffer;
        g_shadowState.vertexFormat = &format;
        WriteWord(CmdBindVertexBuffer);
        WriteWord(buffer);
        WriteWord(&format);
    }
}

}
