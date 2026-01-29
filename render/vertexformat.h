#ifndef VERTEXFORMAT_H
#define VERTEXFORMAT_H

namespace Render
{

constexpr int MaxVertexAttribs = 5;

// try to infer the gl type
constexpr GLenum GLType(const float *) { return GL_FLOAT; }
constexpr GLenum GLType(const Vector2 *) { return GL_FLOAT; }
constexpr GLenum GLType(const Vector3 *) { return GL_FLOAT; }
constexpr GLenum GLType(const Vector4 *) { return GL_FLOAT; }
constexpr GLenum GLType(const int8_t (*)[4]) { return GL_BYTE; }
constexpr GLenum GLType(const uint8_t (*)[4]) { return GL_UNSIGNED_BYTE; }
constexpr GLenum GLType(const uint16_t (*)[2]) { return GL_UNSIGNED_SHORT; }

// try to infer the component count
constexpr int ComponentCount(const float *) { return 1; }
constexpr int ComponentCount(const Vector2 *) { return 2; }
constexpr int ComponentCount(const Vector3 *) { return 3; }
constexpr int ComponentCount(const Vector4 *) { return 4; }
constexpr int ComponentCount(const int8_t (*)[4]) { return 4; }
constexpr int ComponentCount(const uint8_t (*)[4]) { return 4; }
constexpr int ComponentCount(const uint16_t (*)[2]) { return 2; }

struct VertexAttrib
{
    // convenience helper
    template<typename Field, typename Vertex>
    VertexAttrib(const Field Vertex::*ptr, const char *_name, bool _normalized = false)
    {
        const Vertex *object = nullptr;
        const Field *field = &(object->*ptr);
        offset = static_cast<int>(reinterpret_cast<intptr_t>(field));
        type = GLType(field);
        size = ComponentCount(field);
        normalized = _normalized;
        name = _name;
    }

    int offset;
    GLenum type;
    int size;
    bool normalized;
    const char *name; // a_##name
};

struct VertexFormat
{
    int stride;
    Span<const VertexAttrib> attribs;
};

}

#endif
