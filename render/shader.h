#ifndef SHADER_H
#define SHADER_H

namespace Render
{

struct VertexAttrib;

// sigh, no explicit uniform locations with opengl 3.3
struct ShaderUniform
{
    int offset;
    const char *name;
};

// using macros to make this less error prone:
// static const ShaderUniform s_uniforms[] =
// {
//   // "constant" uniform, value gets set after linking
//   SHADER_UNIFORM_CONST(u_texture, 0),
//
//   // normal uniform, u_time being a member variable of ShaderClass
//   SHADER_UNIFORM_MUT(ShaderClass, u_time),
//
//   // terminates the list (nullptr name)
//   SHADER_UNIFORM_TERM()
// };
#define SHADER_UNIFORM_MUT(objectType, field) { (int)offsetof(objectType, field), #field }
#define SHADER_UNIFORM_CONST(field, intergerValue) { -1 - intergerValue, #field }
#define SHADER_UNIFORM_TERM() { 0, nullptr }

union UniformValue
{
    int int_{};
    float float_;
};

// the shader system has suffered trauma and is not optimal, but it's quite simple:
//
// 1. every shader program class inherits from BaseShader, and
//  implements Name, VertexAttribs, and Uniforms
//
// 2. if you want shader variants, inherit from the parent
//  shader class and implement Defines
class BaseShader
{
public:
    const char *Defines()
    {
        return nullptr;
    }

    GLuint m_program{};

    // we're shadowing the default block to greatly reduce the size of command buffers
    std::unordered_map<GLuint, UniformValue> m_uniformState;
};

void shaderInit();
void shaderUpdate(bool forceRecompile = false);
void shaderUpdateGamma(float brightness, float gamma, float lightgamma);

void shaderRegister(const char *name, byte *shaderStruct, const VertexAttrib *attributes, const ShaderUniform *uniforms, const char *defines);

template<typename T>
inline void shaderRegister(T &shader)
{
    static_assert(offsetof(T, m_program) == 0, "wtf");
    static_assert(std::is_base_of<BaseShader, T>::value, "wtf");
    shaderRegister(shader.Name(), reinterpret_cast<byte *>(&shader), shader.VertexAttribs(), shader.Uniforms(), shader.Defines());
}

}

#endif
