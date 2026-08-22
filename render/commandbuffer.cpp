#include "stdafx.h"
#include "commandbuffer.h"
#include "dynamicbuffer.h"

namespace Render
{

// should be enough for anything except the torture maps
constexpr int InitialBufferCapacity = 16834;

// FIXME: this fucking sucks
static_assert(MaxUniformBlocks == 5, "CmdBindUniformBuffer<n> out of date");

enum Command
{
    CmdActiveTexture,

    CmdBindUniformBuffer0,
    CmdBindUniformBuffer1,
    CmdBindUniformBuffer2,
    CmdBindUniformBuffer3,
    CmdBindUniformBuffer4,

    CmdBindTexture2D,
    CmdBindTextureCubeMap,
    CmdBlendFunc,
    CmdDepthFunc,
    CmdDepthMask,

    CmdDrawElementsBaseVertex,

    CmdPolygonOffset,
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

struct FlatUboBinding
{
    const uint8_t *data;
    int size;
    unsigned sequence; // for dirtiness checks
};

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
    g_state.uniformBindCount = 0;
    g_state.uniformBindSkipCount = 0;
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

static void SetUniform(GLint location, const UboMember &member, const uint8_t *base, int count)
{
    const void *ptr = base + member.offset;

    switch (member.type)
    {
    case UboType::Float:
        glUniform1fv(location, count, static_cast<const GLfloat *>(ptr));
        break;
    case UboType::Int:
    case UboType::Bool:
        glUniform1iv(location, count, static_cast<const GLint *>(ptr));
        break;
    case UboType::Vec2:
        glUniform2fv(location, count, static_cast<const GLfloat *>(ptr));
        break;
    case UboType::Vec3:
        glUniform3fv(location, count, static_cast<const GLfloat *>(ptr));
        break;
    case UboType::Vec4:
        glUniform4fv(location, count, static_cast<const GLfloat *>(ptr));
        break;
    case UboType::Mat4:
        glUniformMatrix4fv(location, count, GL_FALSE, static_cast<const GLfloat *>(ptr));
        break;
    case UboType::Mat3x4:
        glUniformMatrix3x4fv(location, count, GL_FALSE, static_cast<const GLfloat *>(ptr));
        break;
    }
}

static int UboTypeSize(UboType type)
{
    switch (type)
    {
    case UboType::Float:
    case UboType::Int:
    case UboType::Bool:
        return 4;
    case UboType::Vec2:
        return 8;
    case UboType::Vec3:
        return 12;
    case UboType::Vec4:
        return 16;
    case UboType::Mat4:
        return 64;
    case UboType::Mat3x4:
        return 48;
    }

    GL3_ASSERT(false);
    return 0;
}

static void FlushFlatUbos(BaseShader *shader, FlatUboBinding *flatUboBindings)
{
    FlatBlock *blocks = shader->flatBlocks;
    if (!blocks)
    {
        return;
    }

    for (int i = 0; i < MaxUniformBlocks; i++)
    {
        FlatBlock &block = blocks[i];
        if (!block.layout)
        {
            // not used by this program
            continue;
        }

        const FlatUboBinding &binding = flatUboBindings[i];
        if (block.sequence == binding.sequence)
        {
            // no change
            continue;
        }

        // yes change... blast the glUniform* calls
        block.sequence = binding.sequence;

        const UboLayout *layout = block.layout;

        for (int j = 0; j < layout->memberCount; j++)
        {
            GLint location = block.locations[j];
            if (location == -1)
            {
                continue;
            }

            const UboMember &member = layout->members[j];

            // clamp for bones
            int count = member.count;
            if (count > 1)
            {
                int maxCount = (binding.size - member.offset) / UboTypeSize(member.type);
                count = Q_min(count, maxCount);
                GL3_ASSERT(count > 0);
            }

            SetUniform(location, member, binding.data, count);
        }
    }
}

void commandExecute()
{
    GL3_ASSERT(s_recording);
    s_recording = false;

    GL3_ASSERT(s_size);
    GL3_ASSERT(s_readOffset == 0);

    // ugh, need to store some state for the flattened ubos
    BaseShader *shader = nullptr;
    FlatUboBinding flatUboBindings[MaxUniformBlocks]{};

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
        case CmdBindUniformBuffer1:
        case CmdBindUniformBuffer2:
        case CmdBindUniformBuffer3:
        case CmdBindUniformBuffer4:
        {
            int binding = cmd - CmdBindUniformBuffer0;
            //GLenum target = ReadWord<GLenum>();
            //GLuint index = ReadWord<GLuint>();
            GLuint buffer = ReadWord<GLuint>();
            GLintptr offset = ReadWord<GLintptr>();
            GLsizeiptr size = ReadWord<GLsizeiptr>();

#ifdef SCHIZO_DEBUG
            g_state.uniformBindCount++;
#endif

            if (GLAD_GL_ARB_uniform_buffer_object)
            {
                glBindBufferRange(GL_UNIFORM_BUFFER, binding, buffer, offset, size);
            }
            else
            {
                // FIXME: bruh
                static unsigned sequence;
                flatUboBindings[binding].data = &g_flatUboData[offset];
                flatUboBindings[binding].size = static_cast<int>(size);
                flatUboBindings[binding].sequence = ++sequence;
            }
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
            if (!GLAD_GL_ARB_uniform_buffer_object)
            {
                FlushFlatUbos(shader, flatUboBindings);
            }

            //GLenum mode = ReadWord<GLenum>();
            GLsizei count = ReadWord<GLsizei>();
            //GLenum type = ReadWord<GLenum>();
            GLsizei offset = ReadWord<GLsizei>();

            if (GLAD_GL_ARB_draw_elements_base_vertex)
            {
                GLint basevertex = ReadWord<GLint>();
                glDrawElementsBaseVertex(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(offset), basevertex);
            }
            else
            {
                // base vertex applied with vertex attribs
                glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(offset));
            }

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

        case CmdUseProgram:
        {
            shader = ReadWord<BaseShader *>();
            glUseProgram(shader->program);
        }
        break;

        case CmdBindVertexBuffer:
        {
            int i;

            GLuint buffer = ReadWord<GLuint>();
            const VertexFormat *format = ReadWord<const VertexFormat *>();
            int baseVertex = GLAD_GL_ARB_draw_elements_base_vertex ? 0 : ReadWord<int>();

            Span<const VertexAttrib> vertexAttribs = format->attribs;
            int vertexStride = format->stride;
            int baseOffset = baseVertex * vertexStride;

            glBindBuffer(GL_ARRAY_BUFFER, buffer);

            for (i = 0; i < vertexAttribs.size(); i++)
            {
                const VertexAttrib &attrib = vertexAttribs[i];

                glEnableVertexAttribArray(i);
                glVertexAttribPointer(i, attrib.size, attrib.type, attrib.normalized, vertexStride, reinterpret_cast<void *>(static_cast<intptr_t>(baseOffset + attrib.offset)));
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

void commandSetUniformData(UniformBlock index, const void *data, int size)
{
    GL3_ASSERT(s_recording);
    GL3_ASSERT(index < UniformBlock::Count);
    GL3_ASSERT(size > 0);

    int slotIndex = static_cast<int>(index);
    UniformSlot &slot = g_shadowState.uniformSlots[slotIndex];

    bool shadowed = (size <= MaxShadowedUniformSize);
    if (shadowed && slot.shadowSize >= size && !memcmp(slot.shadow, data, size))
    {
        // same data is already bound
#ifdef SCHIZO_DEBUG
        g_state.uniformBindSkipCount++;
#endif
        return;
    }

    BufferSpan span = dynamicUniformData(data, size);

    if (shadowed)
    {
        slot.shadowSize = size;
        memcpy(slot.shadow, data, size);
    }
    else
    {
        slot.shadowSize = 0;
    }

    WriteWord(CmdBindUniformBuffer0 + slotIndex);
    //WriteWord(index);
    WriteWord(span.buffer);
    WriteWord(span.byteOffset);
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

void commandDrawElements(GLenum mode, GLsizei count, GLenum type, GLsizei offset)
{
    GL3_ASSERT(s_recording);
    GL3_ASSERT(mode == GL_TRIANGLES);
    GL3_ASSERT(type == GL_UNSIGNED_SHORT);
    GL3_ASSERT(g_shadowState.baseVertex >= 0);

    WriteWord(CmdDrawElementsBaseVertex);
    //WriteWord(mode);
    WriteWord(count);
    //WriteWord(type);
    WriteWord(offset);

    if (GLAD_GL_ARB_draw_elements_base_vertex)
    {
        // base vertex gets specified with the draw call
        WriteWord(g_shadowState.baseVertex);
    }
}

void commandPolygonOffset(GLfloat factor, GLfloat units)
{
    GL3_ASSERT(s_recording);

    WriteWord(CmdPolygonOffset);
    WriteWord(factor);
    WriteWord(units);
}

void commandUseProgram(BaseShader *shader)
{
    GL3_ASSERT(s_recording);

    if (g_shadowState.shader != shader)
    {
        g_shadowState.shader = shader;
        WriteWord(CmdUseProgram);
        WriteWord(shader);
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

void commandBindVertexBuffer(GLuint buffer, const VertexFormat &format, int baseVertex)
{
    if (GLAD_GL_ARB_draw_elements_base_vertex)
    {
        if (g_shadowState.vertexBuffer != buffer || g_shadowState.vertexFormat != &format)
        {
            g_shadowState.vertexBuffer = buffer;
            g_shadowState.vertexFormat = &format;
            WriteWord(CmdBindVertexBuffer);
            WriteWord(buffer);
            WriteWord(&format);
        }

        g_shadowState.baseVertex = baseVertex;
    }
    else
    {
        if (g_shadowState.vertexBuffer != buffer
            || g_shadowState.vertexFormat != &format
            || g_shadowState.baseVertex != baseVertex)
        {
            g_shadowState.vertexBuffer = buffer;
            g_shadowState.vertexFormat = &format;
            g_shadowState.baseVertex = baseVertex;
            WriteWord(CmdBindVertexBuffer);
            WriteWord(buffer);
            WriteWord(&format);
            WriteWord(baseVertex);
        }
    }
}

}
