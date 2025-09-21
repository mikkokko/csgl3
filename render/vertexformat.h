#ifndef VERTEXFORMAT_H
#define VERTEXFORMAT_H

namespace Render
{

constexpr int MaxVertexAttribs = 5;

struct VertexAttrib
{
    int offset;
    GLenum type;
    int size;
    const char *name; // a_##name
};

struct VertexFormat
{
    const VertexAttrib *attribs;
    int stride;
};

// try to infer the gl type
constexpr GLenum GLType(const float *) { return GL_FLOAT; }
constexpr GLenum GLType(const Vector2 *) { return GL_FLOAT; }
constexpr GLenum GLType(const Vector3 *) { return GL_FLOAT; }
constexpr GLenum GLType(const Vector4 *) { return GL_FLOAT; }
constexpr GLenum GLType(const uint8_t (*)[4]) { return GL_UNSIGNED_BYTE; }

// try to infer the component count
constexpr int ComponentCount(const float *) { return 1; }
constexpr int ComponentCount(const Vector2 *) { return 2; }
constexpr int ComponentCount(const Vector3 *) { return 3; }
constexpr int ComponentCount(const Vector4 *) { return 4; }
constexpr int ComponentCount(const uint8_t (*)[4]) { return 4; }

#define VERTEX_ATTRIB(vertexType, name) { (int)offsetof(vertexType, name), GLType(&((vertexType *)0)->name), ComponentCount(&((vertexType *)0)->name), "a_" #name }
#define VERTEX_ATTRIB_TERM() { 0, 0, 0, nullptr }

}

#endif
