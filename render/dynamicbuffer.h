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

class DynamicVertexState
{
public:
    void Lock(int vertexSize);
    void Unlock();

    // index for the vertex at current offset, considering the entire buffer
    int IndexBase() const
    {
        return m_baseVertex + m_offset;
    }

    bool IsLocked() const { return m_data ? true : false; }

    GLuint VertexBuffer() const { return m_bufferHandle; }

    void Write(const void *data, int vertexCount)
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

extern DynamicVertexState g_dynamicVertexState;

class DynamicIndexState
{
public:
    void Lock(int indexSize);
    void Unlock();

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

    void Write(const void *data, int indexCount)
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

extern DynamicIndexState g_dynamicIndexState;

}

#endif
