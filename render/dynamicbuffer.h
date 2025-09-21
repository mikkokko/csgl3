#ifndef DYNAMICBUFFER_H
#define DYNAMICBUFFER_H

namespace Render
{

struct BufferSpan
{
    GLuint buffer;
    int offset; // byte offset in buffer
    void *pinned; // agp pinned memory at offset
};

void dynamicBuffersInit();

void dynamicBuffersMap();
void dynamicBuffersUnmap();

// this can cause redundant copying, but is easier to use
BufferSpan dynamicUniformData(const void *data, int size);

BufferSpan dynamicVertexBegin(int vertexSize, int &maxCapacity);
void dynamicVertexEnd(int vertexSize, int vertexCountWritten);

BufferSpan dynamicIndexBegin(int indexSize, int &maxCapacity);
void dynamicIndexEnd(int indexSize, int indexCountWritten);

class DynamicVertexState
{
public:
    void Map(int vertexSize)
    {
        m_vertexSize = vertexSize;
        BufferSpan span = dynamicVertexBegin(m_vertexSize, m_capacity);
        m_bufferHandle = span.buffer;
        m_baseVertex = span.offset / vertexSize;
        GL3_ASSERT((span.offset % vertexSize) == 0);
        m_data = static_cast<uint8_t *>(span.pinned);

        m_offset = 0;
    }

    void Unmap()
    {
        dynamicVertexEnd(m_vertexSize, m_offset);
        m_data = nullptr; // for IsMapped
    }

    // index for the vertex at current offset, considering the entire buffer
    int IndexBase() const
    {
        return m_baseVertex + m_offset;
    }

    bool IsMapped() const { return m_data ? true : false; }

    GLuint VertexBuffer() const { return m_bufferHandle; }

    void WriteData(const void *data, int vertexCount)
    {
        if (m_offset + vertexCount > m_capacity)
        {
            // FIXME: this can actually hit...
            platformError("Dynamic vertex buffer overflow");
        }

        GL3_ASSERT(m_data);
        memcpy(&m_data[m_offset * m_vertexSize], data, vertexCount * m_vertexSize);
        m_offset += vertexCount;
    }

    // manual writing, caller should do bounds checking but yeah they're not going to
    void *BeginWrite()
    {
        return &m_data[m_offset * m_vertexSize];
    }

    void FinishWrite(int vertexCount)
    {
        m_offset += vertexCount;
    }

private:
    int m_vertexSize{};
    int m_capacity{};
    GLuint m_bufferHandle{};
    int m_baseVertex{};
    uint8_t *m_data{};

    int m_offset{};
};

class DynamicIndexState
{
public:
    void Map(int indexSize)
    {
        GL3_ASSERT(indexSize == 2 || indexSize == 4);

        m_indexSize = indexSize;
        BufferSpan span = dynamicIndexBegin(m_indexSize, m_capacity);
        m_bufferHandle = span.buffer;
        m_byteOffset = span.offset;
        GL3_ASSERT((m_byteOffset % indexSize) == 0);
        m_data = static_cast<uint8_t *>(span.pinned);

        m_offset = 0;
        m_offsetLastDraw = 0;
    }

    void Unmap()
    {
        dynamicIndexEnd(m_indexSize, m_offset);
    }

    GLuint IndexBuffer() const { return m_bufferHandle; }
    int IndexSize() const { return m_indexSize; }
    GLenum IndexType() const { return (m_indexSize == 4) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT; }

    int UpdateDrawOffset(int &byteOffset)
    {
        int indexCount = m_offset - m_offsetLastDraw;
        GL3_ASSERT(indexCount >= 0);
        if (!indexCount)
        {
            byteOffset = 0;
            return 0;
        }

        byteOffset = m_byteOffset + (m_offsetLastDraw * m_indexSize);
        m_offsetLastDraw = m_offset;

        return indexCount;
    }

    void WriteData(const void *data, int indexCount)
    {
        if (m_offset + indexCount > m_capacity)
        {
            // FIXME: this can actually hit...
            platformError("Dynamic index buffer overflow");
        }

        GL3_ASSERT(m_data);
        memcpy(&m_data[m_offset * m_indexSize], data, indexCount * m_indexSize);
        m_offset += indexCount;
    }

    // manual writing, caller should do bounds checking but yeah they're not going to
    void *BeginWrite()
    {
        return &m_data[m_offset * m_indexSize];
    }

    void FinishWrite(int indexCount)
    {
        m_offset += indexCount;
    }

private:
    int m_indexSize{};
    int m_capacity{};
    GLuint m_bufferHandle{};
    int m_byteOffset{};
    uint8_t *m_data{};

    int m_offset{};
    int m_offsetLastDraw{};
};

}

#endif
