#ifndef SHADER_H
#define SHADER_H

namespace Render
{

struct VertexAttrib;

struct ShaderUniform
{
    // mutable value: location will be stored at "field"
    template<typename Field, typename Struct>
    ShaderUniform(const char *_name, const Field Struct::*ptr)
    {
        const Struct *object = nullptr;
        const Field *field = &(object->*ptr);
        offset = static_cast<int>(reinterpret_cast<intptr_t>(field));
        name = _name;
    }

    // constant value: set after linking, not to be changed after
    ShaderUniform(const char *_name, int constantValue)
    {
        offset = -1 - constantValue;
        name = _name;
    }

    int offset;
    const char *name;
};

struct ShaderOption
{
    const char *name;
    int maxValue;
};

// for consistency i gues...
#define SHADER_OPTION(name, maxValue) { #name, maxValue }
#define SHADER_OPTION_TERM() { nullptr, 0 }

union UniformValue
{
    int int_{};
    float float_;
};

struct BaseShader
{
    GLuint program;

    // we're shadowing the default block to greatly reduce the size of command buffers
    std::unordered_map<GLuint, UniformValue> uniformState;
};

void shaderInit();
void shaderUpdate(bool forceRecompile = false);
void shaderUpdateGamma(float brightness, float gamma, float lightgamma);

void shaderRegister(
    byte *shaderStructs,
    int shaderStructSize,
    int shaderCount,
    const char *name,
    Span<const VertexAttrib> attributes,
    Span<const ShaderUniform> uniforms,
    Span<const ShaderOption> options);

template<typename T, int ShaderCount>
void shaderRegister(
    T (&shaderStructs)[ShaderCount],
    const char *name,
    Span<const VertexAttrib> attributes,
    Span<const ShaderUniform> uniforms,
    Span<const ShaderOption> options)
{
    // scummy ass test for BaseShader inheritance
    GL3_ASSERT(offsetof(T, program) == 0);
    GL3_ASSERT(offsetof(T, uniformState) == sizeof(GLuint));
    shaderRegister(reinterpret_cast<byte *>(shaderStructs), sizeof(T), ShaderCount, name, attributes, uniforms, options);
}

template<typename T>
void shaderRegister(
    T &shaderStruct,
    const char *name,
    Span<const VertexAttrib> attributes,
    Span<const ShaderUniform> uniforms)
{
    // scummy ass test for BaseShader inheritance
    GL3_ASSERT(offsetof(T, program) == 0);
    GL3_ASSERT(offsetof(T, uniformState) == sizeof(GLuint));
    shaderRegister(reinterpret_cast<byte *>(&shaderStruct), sizeof(T), 1, name, attributes, uniforms, {});
}

template<size_t N>
constexpr int shaderVariantCount(const ShaderOption (&options)[N])
{
    int count = 1;

    for (const ShaderOption &option : options)
    {
        count *= (option.maxValue + 1);
    }

    if (count > 99)
    {
        // wtf
        return -1;
    }

    return count;
}

template<typename S, int ShaderCount, typename T, int OptionCount>
S &shaderSelect(S (&shaders)[ShaderCount], const ShaderOption (&optionInfo)[OptionCount], const T &options)
{
    static_assert((sizeof(options) / sizeof(int)) == OptionCount, "Option structure size mismatch");
    const int *values = reinterpret_cast<const int *>(&options);

    int index = 0;
    int accum = 1;

    for (int i = 0; i < OptionCount; i++)
    {
        GL3_ASSERT(values[i] <= optionInfo[i].maxValue);
        index += (accum * values[i]);
        accum *= (optionInfo[i].maxValue + 1);
    }

    GL3_ASSERT(index < ShaderCount);
    return shaders[index];
}

}

#endif
