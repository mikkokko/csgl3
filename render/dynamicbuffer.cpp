#include "stdafx.h"
#include "dynamicbuffer.h"

namespace Render
{

// we're doing manual triple buffering
constexpr int BufferCount = 3;

struct GLBuffer
{
    GLuint handle;
    uint8_t *mapped;
};

class DynamicBuffer
{
    const GLenum m_target;
    const int m_bufferSize;

    int m_offset{};
    GLBuffer m_buffers[BufferCount]{};

#ifdef SCHIZO_DEBUG
    bool m_writingRegion{ false };
    int m_elementSize{ -1 };
#endif

public:
    DynamicBuffer(GLenum target, const int byteSize)
        : m_target{ target }
        , m_bufferSize{ byteSize }
    {
    }

    void Init()
    {
        for (GLBuffer &buffer : m_buffers)
        {
            glGenBuffers(1, &buffer.handle);
            glBindBuffer(m_target, buffer.handle);
            glBufferData(m_target, m_bufferSize, nullptr, GL_STREAM_DRAW);
        }
    }

    void Map(int index)
    {
        GL3_ASSERT(index >= 0 && index < BufferCount);
        GLBuffer &buffer = m_buffers[index];
        GL3_ASSERT(!buffer.mapped);

        glBindBuffer(m_target, buffer.handle);
        void *mapped = glMapBufferRange(m_target, 0, m_bufferSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
        GL3_ASSERT(mapped);

        buffer.mapped = static_cast<uint8_t *>(mapped);
        GL3_ASSERT(m_offset == 0);
    }

    int m_maxSize{};

    void Unmap(int index)
    {
        GL3_ASSERT(index >= 0 && index < BufferCount);
        for (int i = 0; i < BufferCount; i++)
        {
            if (i != index)
            {
                GL3_ASSERT(!m_buffers[i].mapped);
            }
        }

        GLBuffer &buffer = m_buffers[index];
        GL3_ASSERT(buffer.mapped);

        glBindBuffer(m_target, buffer.handle);
        glFlushMappedBufferRange(m_target, 0, m_offset);
        glUnmapBuffer(m_target);
        buffer.mapped = nullptr;

#ifdef SCHIZO_DEBUG
        switch (m_target)
        {
        case GL_ARRAY_BUFFER:
            g_state.vertexBufferSize = m_offset;
            break;

        case GL_ELEMENT_ARRAY_BUFFER:
            g_state.indexBufferSize = m_offset;
            break;

        case GL_UNIFORM_BUFFER:
            g_state.uniformBufferSize = m_offset;
            break;
        }
#endif

        m_maxSize = Q_max(m_offset, m_maxSize);

        m_offset = 0;
    }

    BufferSpan Upload(int index, const void *data, int size, int alignment)
    {
        GL3_ASSERT(m_writingRegion == 0);

        m_offset = AlignUp(m_offset, alignment);

        if (m_offset + size > m_bufferSize)
        {
            platformError("Dynamic GPU buffer overflow");
        }

        GLBuffer &buffer = m_buffers[index];

        GL3_ASSERT(buffer.mapped);
        memcpy(&buffer.mapped[m_offset], data, size);

        BufferSpan span;
        span.buffer = buffer.handle;
        span.offset = m_offset;

        m_offset += size;

        return span;
    }

    BufferSpan BeginRegion(int index, int elementSize, int &maxElementCount)
    {
        m_offset = AlignUp(m_offset, elementSize);

        int availableBytes = m_bufferSize - m_offset;
        maxElementCount = availableBytes / elementSize;
        if (maxElementCount <= 0)
        {
            platformError("Dynamic GPU buffer overflow");
        }

        GLBuffer &buffer = m_buffers[index];

        BufferSpan span;
        span.buffer = buffer.handle;
        span.offset = m_offset;
        span.pinned = &buffer.mapped[m_offset];

        // debug slop
#ifdef SCHIZO_DEBUG
        GL3_ASSERT(m_writingRegion == false);
        GL3_ASSERT(m_elementSize == -1);
        m_writingRegion = true;
        m_elementSize = elementSize;
#endif

        return span;
    }

    void EndRegion(int elementSize, int elementCount)
    {
        // debug slop
#ifdef SCHIZO_DEBUG
        GL3_ASSERT(m_writingRegion == true);
        GL3_ASSERT(m_elementSize == elementSize);
        m_writingRegion = false;
        m_elementSize = -1;
#endif

        // m_offset was aligned by BeginRegion
        int bytesWritten = elementCount * elementSize;
        GL3_ASSERT((m_offset % elementSize) == 0);
        GL3_ASSERT(m_offset + bytesWritten <= m_bufferSize);
        m_offset += bytesWritten;
    }
};

// current index of the dynamic buffers, so [0, BufferCount[
static int s_bufferFrame;

static int s_uniformBufferOffsetAlignment;

static DynamicBuffer s_vertex{ GL_ARRAY_BUFFER, 1 << 17 };
static DynamicBuffer s_index{ GL_ELEMENT_ARRAY_BUFFER, 1 << 19 };
static DynamicBuffer s_uniform{ GL_UNIFORM_BUFFER, 1 << 17 };

void dynamicBuffersInit()
{
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &s_uniformBufferOffsetAlignment);

    s_vertex.Init();
    s_index.Init();
    s_uniform.Init();
}

void dynamicBuffersMap()
{
    s_vertex.Map(s_bufferFrame);
    s_index.Map(s_bufferFrame);
    s_uniform.Map(s_bufferFrame);
}

void dynamicBuffersUnmap()
{
    s_vertex.Unmap(s_bufferFrame);
    s_index.Unmap(s_bufferFrame);
    s_uniform.Unmap(s_bufferFrame);
    s_bufferFrame = (s_bufferFrame + 1) % BufferCount;
}

BufferSpan dynamicUniformData(const void *data, int size)
{
    return s_uniform.Upload(s_bufferFrame, data, size, s_uniformBufferOffsetAlignment);
}

BufferSpan dynamicVertexBegin(int vertexSize, int &maxCapacity)
{
    return s_vertex.BeginRegion(s_bufferFrame, vertexSize, maxCapacity);
}

void dynamicVertexEnd(int vertexSize, int vertexCountWritten)
{
    s_vertex.EndRegion(vertexSize, vertexCountWritten);
}

BufferSpan dynamicIndexBegin(int indexSize, int &maxCapacity)
{
    return s_index.BeginRegion(s_bufferFrame, indexSize, maxCapacity);
}

void dynamicIndexEnd(int indexSize, int indexCountWritten)
{
    s_index.EndRegion(indexSize, indexCountWritten);
}

}
