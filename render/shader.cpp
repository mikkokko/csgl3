#include "stdafx.h"
#include "shader.h"

// enable this if you want to reload shaders at runtime
//#define SHADER_RELOAD

#ifdef SHADER_RELOAD
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL
#include "stb_include.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#else
struct ShaderData
{
    const char *name;
    const void *data;
    int size;
};

#include SHADER_SOURCES_FILE
#endif

namespace Render
{

constexpr int MaxRegisteredShaders = 16;

struct ShaderInfo
{
    const char *name;

    byte *instanceData;
    int instanceDataSize;
    int variantCount;

    Span<const VertexAttrib> attributes;
    Span<const ShaderUniform> uniforms;
    Span<const ShaderOption> options;
};

struct CachedShader
{
    GLuint handle{};
    int lastUsedGeneration{};
};

struct ShaderManagerState
{
    // FIXME: set sane defaults instead?
    float brightness = -1.0f;
    float gamma = -1.0f;
    float lightgamma = -1.0f;

    bool recompileQueued{ true };

    int cacheGeneration{};
    std::unordered_map<std::string, CachedShader> shaderCache;

    int registeredCount{};
    ShaderInfo registeredShaders[MaxRegisteredShaders];
};

static ShaderManagerState s_state;

static const char *GetShaderTypeString(GLenum type)
{
    switch (type)
    {
    case GL_FRAGMENT_SHADER:
        return "fragment";
    case GL_VERTEX_SHADER:
        return "vertex";
    default:
        return "unknown";
    }
}

static void LoadRawSource(const char *name, std::string &outSource)
{
#ifdef SHADER_RELOAD
    char error[256];
    char *data = stb_include_file((char *)name, nullptr, SHADER_PATH, error);
    if (!data)
    {
        platformError("%s", error);
        return;
    }

    outSource.assign(data);
    free(data);
#else
    for (const ShaderData &entry : s_shaderData)
    {
        if (!strcmp(entry.name, name))
        {
            outSource.assign(reinterpret_cast<const char *>(entry.data), entry.size);
            return;
        }
    }

    platformError("No such shader embedded: %s", name);
#endif
}

static void LoadShaderPairSource(const char *shaderName, std::string &outVert, std::string &outFrag)
{
    char vertName[256];
    char fragName[256];

#ifdef SHADER_RELOAD
    snprintf(vertName, sizeof(vertName), SHADER_PATH "/%s.vert", shaderName);
    snprintf(fragName, sizeof(fragName), SHADER_PATH "/%s.frag", shaderName);
#else
    snprintf(vertName, sizeof(vertName), "%s.vert", shaderName);
    snprintf(fragName, sizeof(fragName), "%s.frag", shaderName);
#endif

    LoadRawSource(vertName, outVert);
    LoadRawSource(fragName, outFrag);
}

static GLuint CompileShader(const char *shaderName, const std::string &sourceString, GLenum type)
{
    const char *sourcePtr = sourceString.c_str();
    int length = static_cast<int>(sourceString.size());

    GLuint shaderHandle = glCreateShader(type);
    glShaderSource(shaderHandle, 1, &sourcePtr, &length);
    glCompileShader(shaderHandle);

    GLint status = 0;
    glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &status);

    if (!status)
    {
        char log[1024];
        glGetShaderInfoLog(shaderHandle, sizeof(log), nullptr, log);
        platformError("Compiling %s %s shader failed:\n%s",
            shaderName, GetShaderTypeString(type), log);
    }
#ifdef SCHIZO_DEBUG
    else
    {
        char log[1024];
        glGetShaderInfoLog(shaderHandle, sizeof(log), nullptr, log);
        if (log[0])
        {
            platformError("%s %s shader warning:\n%s",
                shaderName, GetShaderTypeString(type), log);
        }
    }
#endif

    return shaderHandle;
}

static GLuint GetOrCompileShader(const char *name, const std::string &fullSource, GLenum type)
{
    CachedShader &entry = s_state.shaderCache[fullSource];

    entry.lastUsedGeneration = s_state.cacheGeneration;

    if (!entry.handle)
    {
        entry.handle = CompileShader(name, fullSource, type);
    }

    return entry.handle;
}

template<typename T>
static void AddMacro(std::string &buffer, const std::string &baseSource, const char *macroName, const T &value)
{
    if (value == 0)
    {
        // if the value is 0, might as well not define it
        return;
    }

    // if you remove this, the shader cache won't work
    if (baseSource.find(macroName) == std::string::npos)
    {
        // macro not used, so don't define it
        return;
    }

    buffer.append("#define ");
    buffer.append(macroName);
    buffer.append(" ");
    buffer.append(std::to_string(value));
    buffer.append("\n");
}

static std::string GenerateVariantSource(const std::string &baseSource, Span<const ShaderOption> options, int variantIndex)
{
    std::string source;
    source.reserve(baseSource.size() + 256);

    source.append("#version 140\n");

    AddMacro(source, baseSource, "V_BRIGHTNESS", s_state.brightness);
    AddMacro(source, baseSource, "V_GAMMA", s_state.gamma);
    AddMacro(source, baseSource, "V_LIGHTGAMMA", s_state.lightgamma);

    int combination = variantIndex;
    for (const ShaderOption &opt : options)
    {
        int range = opt.maxValue + 1;
        int val = (combination % range);
        combination /= range;

        AddMacro(source, baseSource, opt.name, val);
    }

    source.append(baseSource);
    return source;
}

static void SetupUniforms(GLuint program, byte *instancePtr, Span<const ShaderUniform> uniforms)
{
    glUseProgram(program);

    for (const ShaderUniform &uniform : uniforms)
    {
        if (uniform.offset < 0)
        {
            GLint location = glGetUniformLocation(program, uniform.name);
            if (location != -1)
            {
                int value = -uniform.offset - 1;
                glUniform1i(location, value);
            }
        }
        else
        {
            GLint *locationPtr = reinterpret_cast<GLint *>(instancePtr + uniform.offset);
            *locationPtr = glGetUniformLocation(program, uniform.name);
        }
    }

    glUseProgram(0);
}

static void BindUniformBlocks(GLuint program)
{
    struct BlockBinding
    {
        const char *name;
        int binding;
    };

    static const BlockBinding blocks[] = {
        { "FrameConstants", 0 },
        { "ModelConstants", 1 },
        { "FogConstants", 2 }
    };

    for (const BlockBinding &block : blocks)
    {
        GLuint index = glGetUniformBlockIndex(program, block.name);
        if (index != GL_INVALID_INDEX)
        {
            glUniformBlockBinding(program, index, block.binding);
        }
    }
}

static GLuint LinkShaderProgram(const char *name, GLuint vs, GLuint fs, Span<const VertexAttrib> attribs)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);

    for (int i = 0; i < attribs.size(); i++)
    {
        glBindAttribLocation(program, i, attribs[i].name);
    }

    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        platformError("Linking failed for %s:\n%s", name, log);
    }

    glDetachShader(program, vs);
    glDetachShader(program, fs);

    return program;
}

static void BuildShaderVariant(const ShaderInfo &info, const std::string &baseVertSrc, const std::string &baseFragSrc, int variantIndex)
{
    byte *instancePtr = &info.instanceData[info.instanceDataSize * variantIndex];

    // m_program is guaranteed to be at offset 0
    GLuint *programPtr = reinterpret_cast<GLuint *>(instancePtr);
    if (*programPtr)
    {
        glDeleteProgram(*programPtr);
        *programPtr = 0;
    }

    std::string fullVertSrc = GenerateVariantSource(baseVertSrc, info.options, variantIndex);
    std::string fullFragSrc = GenerateVariantSource(baseFragSrc, info.options, variantIndex);

    GLuint vertShader = GetOrCompileShader(info.name, fullVertSrc, GL_VERTEX_SHADER);
    GLuint fragShader = GetOrCompileShader(info.name, fullFragSrc, GL_FRAGMENT_SHADER);

    GLuint program = LinkShaderProgram(info.name, vertShader, fragShader, info.attributes);
    *programPtr = program;

    BindUniformBlocks(program);
    SetupUniforms(program, instancePtr, info.uniforms);
}

#ifdef SHADER_RELOAD
static void ShaderReload()
{
    shaderUpdate(true);
}
#endif

void shaderInit()
{
#ifdef SHADER_RELOAD
    g_engfuncs.pfnAddCommand("gl3_shader_reload", ShaderReload);
#endif
}

void shaderUpdate(bool forceRecompile)
{
    if (!s_state.recompileQueued && !forceRecompile)
    {
        return;
    }

    s_state.recompileQueued = false;

    double startTime = g_engfuncs.GetAbsoluteTime();
    g_engfuncs.Con_Printf("Shader recompile triggered\n");

    s_state.cacheGeneration++;

    for (int i = 0; i < s_state.registeredCount; i++)
    {
        const ShaderInfo &info = s_state.registeredShaders[i];

        std::string baseVert, baseFrag;
        LoadShaderPairSource(info.name, baseVert, baseFrag);

        for (int v = 0; v < info.variantCount; v++)
        {
            BuildShaderVariant(info, baseVert, baseFrag, v);
        }
    }

    // delete shaders that are not used by the current programs (probably won't be used by future programs either)
    for (auto it = s_state.shaderCache.begin(); it != s_state.shaderCache.end();)
    {
        const CachedShader &entry = it->second;
        if (entry.lastUsedGeneration != s_state.cacheGeneration)
        {
            glDeleteShader(entry.handle);
            it = s_state.shaderCache.erase(it);
        }
        else
        {
            it++;
        }
    }

    double endTime = g_engfuncs.GetAbsoluteTime();
    g_engfuncs.Con_Printf("Shader recompile took %g ms\n", (endTime - startTime) * 1000.0);
}

void shaderUpdateGamma(float brightness, float gamma, float lightgamma)
{
    if (s_state.brightness != brightness || s_state.gamma != gamma || s_state.lightgamma != lightgamma)
    {
        s_state.brightness = brightness;
        s_state.gamma = gamma;
        s_state.lightgamma = lightgamma;
        s_state.recompileQueued = true;
    }
}

void shaderRegister(
    byte *shaderStructs,
    int shaderStructSize,
    int shaderCount,
    const char *name,
    Span<const VertexAttrib> attributes,
    Span<const ShaderUniform> uniforms,
    Span<const ShaderOption> options)
{
    GL3_ASSERT(s_state.registeredCount < MaxRegisteredShaders);

    ShaderInfo &info = s_state.registeredShaders[s_state.registeredCount++];
    info.name = name;
    info.instanceData = shaderStructs;
    info.instanceDataSize = shaderStructSize;
    info.variantCount = shaderCount;
    info.attributes = attributes;
    info.uniforms = uniforms;
    info.options = options;
}

}
