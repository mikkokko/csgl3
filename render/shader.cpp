#include "stdafx.h"
#include "shader.h"
#include "lightgamma.h"

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
#endif

namespace Render
{

#ifndef SHADER_RELOAD
struct ShaderData
{
    const char *name;
    const void *data;
    int size;
};

struct ShaderData120
{
    const char *name;
    const void *data;
    int size;

    const UboLayout *const *ubos;
    int uboCount;
};

#include SHADER_SOURCES_FILE
#endif

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
    bool overbright = false;

    bool recompileQueued{ true };

    int cacheGeneration{};
    std::unordered_map<std::string, CachedShader> shaderCache;

    int registeredCount{};
    ShaderInfo registeredShaders[MaxRegisteredShaders]{};
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
    if (GLAD_GL_ARB_uniform_buffer_object)
    {
        for (const ShaderData &entry : s_shaderData)
        {
            if (!strcmp(entry.name, name))
            {
                outSource.assign(reinterpret_cast<const char *>(entry.data), entry.size);
                return;
            }
        }
    }
    else
    {
        for (const ShaderData120 &entry : s_shaderData_120)
        {
            if (!strcmp(entry.name, name))
            {
                outSource.assign(reinterpret_cast<const char *>(entry.data), entry.size);
                return;
            }
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
            g_engfuncs.Con_Printf("%s %s shader log:\n%s",
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

static void AddMacro(std::string &buffer, const std::string &baseSource, const char *macroName, int value)
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

static void AddMacro(std::string &buffer, const std::string &baseSource, const char *macroName, float value)
{
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

    // if ubos are not available, we need to use the glsl 1.20
    // shaders that have them converted to plain uniforms
    if (!GLAD_GL_ARB_uniform_buffer_object)
    {
        source.append("#version 120\n");
    }
    else
    {
        source.append("#version 140\n");
    }

    AddMacro(source, baseSource, "V_BRIGHTNESS", s_state.brightness);
    AddMacro(source, baseSource, "V_GAMMA", s_state.gamma);
    AddMacro(source, baseSource, "V_LIGHTGAMMA", s_state.lightgamma);

    AddMacro(source, baseSource, "OVERBRIGHT", s_state.overbright ? 1 : 0);

    // kludge for brush.frag
    AddMacro(source, baseSource, "V_LIGHTGAMMA_2X", powf(2.0f, 1.0f / s_state.lightgamma));

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

static void SetupUniforms(const char *shaderName, GLuint program, byte *instancePtr, Span<const ShaderUniform> uniforms)
{
    glUseProgram(program);

    for (const ShaderUniform &uniform : uniforms)
    {
        GLint location = glGetUniformLocation(program, uniform.name);
        if (location != -1)
        {
            glUniform1i(location, uniform.samplerSlot);
        }
    }

    glUseProgram(0);
}

static const char *const s_blockNames[] = {
    "FrameConstants",
    "FogConstants",
    "BoneConstants",
    "ModelConstants",
    "TextureConstants"
};

static_assert(std::size(s_blockNames) == MaxUniformBlocks, "bruh");

static void BindUniformBlocks(GLuint program)
{
    for (int binding = 0; binding < MaxUniformBlocks; binding++)
    {
        GLuint index = glGetUniformBlockIndex(program, s_blockNames[binding]);
        if (index != GL_INVALID_INDEX)
        {
            glUniformBlockBinding(program, index, binding);
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

#ifndef SHADER_RELOAD
static int BindingForBlock(const char *blockName)
{
    for (int binding = 0; binding < MaxUniformBlocks; binding++)
    {
        if (!strcmp(blockName, s_blockNames[binding]))
        {
            return binding;
        }
    }

    platformError("Unknown uniform block %s", blockName);
}

// FIXME: this sucks!!!
static const ShaderData120 &FindShaderData120(const char *name)
{
    for (const ShaderData120 &entry : s_shaderData_120)
    {
        if (!strcmp(entry.name, name))
        {
            return entry;
        }
    }

    platformError("No such shader embedded: %s", name);
}

static FlatBlock *BuildFlatBlocks(GLuint program, const char *shaderName)
{
    // FIXME: unfuck this!!!
    char vertName[256];
    char fragName[256];

    snprintf(vertName, sizeof(vertName), "%s.vert", shaderName);
    snprintf(fragName, sizeof(fragName), "%s.frag", shaderName);

    const ShaderData120 &vertData = FindShaderData120(vertName);
    const ShaderData120 &fragData = FindShaderData120(fragName);

    const UboLayout *layouts[MaxUniformBlocks]{};
    int memberCount = 0;

    for (const ShaderData120 *data : { &vertData, &fragData })
    {
        for (int i = 0; i < data->uboCount; i++)
        {
            const UboLayout *layout = data->ubos[i];
            int binding = BindingForBlock(layout->blockName);

            if (layouts[binding])
            {
                GL3_ASSERT(layouts[binding] == layout);
                continue;
            }

            layouts[binding] = layout;
            memberCount += layout->memberCount;
        }
    }

    if (!memberCount)
    {
        return nullptr;
    }

    // hellish malloc
    // FIXME: this can be unfucked
    constexpr int blocksSize = MaxUniformBlocks * sizeof(FlatBlock);
    int locationsSize = memberCount * sizeof(GLint);

    uint8_t *base = static_cast<uint8_t *>(malloc(blocksSize + locationsSize));
    FlatBlock *blocks = reinterpret_cast<FlatBlock *>(base);
    GLint *locations = reinterpret_cast<GLint *>(base + blocksSize);

    memset(blocks, 0, blocksSize);

    for (int binding = 0; binding < MaxUniformBlocks; binding++)
    {
        const UboLayout *layout = layouts[binding];
        if (!layout)
        {
            continue;
        }

        FlatBlock &block = blocks[binding];
        block.layout = layout;
        block.locations = locations;

        for (int i = 0; i < layout->memberCount; i++)
        {
            *locations++ = glGetUniformLocation(program, layout->members[i].name);
        }
    }

    return blocks;
}
#endif

static void BuildShaderVariant(const ShaderInfo &info, const std::string &baseVertSrc, const std::string &baseFragSrc, const std::string &prologue, int variantIndex)
{
    // creepy! but it'll work
    byte *instancePtr = &info.instanceData[info.instanceDataSize * variantIndex];
    BaseShader *instance = reinterpret_cast<BaseShader *>(instancePtr);

    if (instance->program)
    {
        glDeleteProgram(instance->program);
        instance->program = 0;
    }

    free(instance->flatBlocks);
    instance->flatBlocks = nullptr;

    std::string fullVertSrc = GenerateVariantSource(prologue + baseVertSrc, info.options, variantIndex);
    std::string fullFragSrc = GenerateVariantSource(prologue + baseFragSrc, info.options, variantIndex);

    GLuint vertShader = GetOrCompileShader(info.name, fullVertSrc, GL_VERTEX_SHADER);
    GLuint fragShader = GetOrCompileShader(info.name, fullFragSrc, GL_FRAGMENT_SHADER);

    GLuint program = LinkShaderProgram(info.name, vertShader, fragShader, info.attributes);
    instance->program = program;

    if (GLAD_GL_ARB_uniform_buffer_object)
    {
        BindUniformBlocks(program);
    }

    SetupUniforms(info.name, program, instancePtr, info.uniforms);

#ifndef SHADER_RELOAD
    if (!GLAD_GL_ARB_uniform_buffer_object)
    {
        instance->flatBlocks = BuildFlatBlocks(program, info.name);
    }
#endif
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

    g_engfuncs.Con_Printf("Shader recompile triggered\n");

    s_state.cacheGeneration++;

    // fit the lightgamma curve (FIXME)
    std::string lightgammaSource = LightGammaGLSL(s_state.gamma, s_state.lightgamma, s_state.brightness);

    for (int i = 0; i < s_state.registeredCount; i++)
    {
        const ShaderInfo &info = s_state.registeredShaders[i];

        std::string baseVert, baseFrag;
        LoadShaderPairSource(info.name, baseVert, baseFrag);

        for (int v = 0; v < info.variantCount; v++)
        {
            BuildShaderVariant(info, baseVert, baseFrag, lightgammaSource, v);
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
}

void shaderUpdateGamma(float brightness, float gamma, float lightgamma, bool overbright)
{
    if (s_state.brightness != brightness || s_state.gamma != gamma || s_state.lightgamma != lightgamma || s_state.overbright != overbright)
    {
        s_state.brightness = brightness;
        s_state.gamma = gamma;
        s_state.lightgamma = lightgamma;
        s_state.overbright = overbright;
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
