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

constexpr int MaxShaderInfo = 16;

struct ShaderInfo
{
    const char *name;
    byte *shaderStruct;
    const VertexAttrib *attributes;
    const ShaderUniform *uniforms;
    const char *defines;
};

// FIXME: set sane defaults instead?
static float s_brightness = -1;
static float s_gamma = -1;
static float s_lightgamma = -1;

static bool s_recompileQueued = true;

static int s_shaderCount;
static ShaderInfo s_shaders[MaxShaderInfo];

static const char *ShaderTypeString(GLenum type)
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

static GLuint CompileShader(const char *shaderName, std::string &sourceString, GLenum type)
{
    const char *source = sourceString.c_str();
    int length = sourceString.size();

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, &length);
    glCompileShader(shader);

    GLint wasCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &wasCompiled);

    if (!wasCompiled)
    {
        char message[1024];
        glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
        platformError("Compiling %s %s shader failed:\n%s", shaderName, ShaderTypeString(type), message);
    }
#ifdef SCHIZO_DEBUG
    else
    {
        char message[1024];
        glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
        if (message[0])
        {
            platformError("%s %s shader compilation message:\n%s", shaderName, ShaderTypeString(type), message);
        }
    }
#endif

    return shader;
}

#ifdef SHADER_RELOAD
static void LoadFileFromDisk(const char *name, std::string &result)
{
    char error[256];
    char *data = stb_include_file((char *)name, NULL, SHADER_PATH, error);
    if (!data)
    {
        platformError("%s", error);
    }

    result.assign(data);
    free(data);
}
#else
static void GetShaderSource(const char *name, std::string &result)
{
    for (const ShaderData &shader : s_shaderData)
    {
        if (!strcmp(shader.name, name))
        {
            result.assign(reinterpret_cast<const char *>(shader.data), shader.size);
            return;
        }
    }

    platformError("No such shader %s", name);
}
#endif

static void LoadShaderSources(const char *name, std::string &vertexSource, std::string &fragmentSource)
{
#ifdef SHADER_RELOAD
    char vertexShaderPath[256], fragmentShaderPath[256];
    Q_sprintf(vertexShaderPath, SHADER_PATH "/%s.vert", name);
    Q_sprintf(fragmentShaderPath, SHADER_PATH "/%s.frag", name);

    LoadFileFromDisk(vertexShaderPath, vertexSource);
    LoadFileFromDisk(fragmentShaderPath, fragmentSource);
#else
    char vertexShaderName[256], fragmentShaderName[256];
    Q_sprintf(vertexShaderName, "%s.vert", name);
    Q_sprintf(fragmentShaderName, "%s.frag", name);

    GetShaderSource(vertexShaderName, vertexSource);
    GetShaderSource(fragmentShaderName, fragmentSource);
#endif
}

template<typename T>
static void AppendDefine(std::string &buffer, const char *name, const T &value)
{
    buffer.append("#define ");
    buffer.append(name);
    buffer.append(" ");
    buffer.append(std::to_string(value));
    buffer.append("\n");
}

static GLuint LoadShaderVariant(
    const char *shaderName,
    std::string &vertexSourceBase,
    std::string &fragmentSourceBase,
    byte *shaderStruct,
    const VertexAttrib *attributes,
    const ShaderUniform *uniforms,
    const char *defines,
    float brightness,
    float gamma,
    float lightgamma)
{
    std::string prologue{ "#version 140\n" };

    AppendDefine(prologue, "V_BRIGHTNESS", brightness);
    AppendDefine(prologue, "V_GAMMA", gamma);
    AppendDefine(prologue, "V_LIGHTGAMMA", lightgamma);

    if (defines)
    {
        prologue.append(defines);
    }

    std::string vertexSource{ prologue };
    vertexSource.append(vertexSourceBase);

    std::string fragmentSource{ prologue };
    fragmentSource.append(fragmentSourceBase);

    GLuint vertexShader = CompileShader(shaderName, vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = CompileShader(shaderName, fragmentSource, GL_FRAGMENT_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    for (int i = 0; attributes[i].name; i++)
    {
        glBindAttribLocation(program, i, attributes[i].name);
    }

    glLinkProgram(program);

    GLint wasLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &wasLinked);
    if (!wasLinked)
    {
        char message[1024];
        glGetProgramInfoLog(program, sizeof(message), nullptr, message);
        platformError("glLinkProgram failed for %s:\n%s", shaderName, message);
    }

    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(program);
    for (int i = 0; uniforms[i].name; i++)
    {
        if (uniforms[i].offset < 0)
        {
            // constant integer value
            GLint location = glGetUniformLocation(program, uniforms[i].name);
            if (location != -1)
            {
                int value = -uniforms[i].offset - 1;
                glUniform1i(location, value);
            }
        }
        else
        {
            GLint &location = *reinterpret_cast<GLint *>(shaderStruct + uniforms[i].offset);
            location = glGetUniformLocation(program, uniforms[i].name);
        }
    }
    glUseProgram(0);

    return program;
}

static void SetConstantBufferBindings(const char *shaderName, GLuint program)
{
    GLuint frameIndex = glGetUniformBlockIndex(program, "FrameConstants");
    if (frameIndex != GL_INVALID_INDEX)
    {
        glUniformBlockBinding(program, frameIndex, 0);
    }

    GLuint modelIndex = glGetUniformBlockIndex(program, "ModelConstants");
    if (modelIndex != GL_INVALID_INDEX)
    {
        glUniformBlockBinding(program, modelIndex, 1);
    }

    GLuint fogIndex = glGetUniformBlockIndex(program, "FogConstants");
    if (fogIndex != GL_INVALID_INDEX)
    {
        glUniformBlockBinding(program, fogIndex, 2);
    }
}

void LoadShaderFromInfo(const ShaderInfo &info, float brightness, float gamma, float lightgamma)
{
    std::string vertexSource, fragmentSource;
    LoadShaderSources(info.name, vertexSource, fragmentSource);

    // m_program is guaranteed to be at offset 0
    GLuint *pprogram = reinterpret_cast<GLuint *>(info.shaderStruct);
    if (*pprogram)
    {
        glDeleteProgram(*pprogram);
        *pprogram = 0;
    }

    *pprogram = LoadShaderVariant(
        info.name,
        vertexSource,
        fragmentSource,
        info.shaderStruct,
        info.attributes,
        info.uniforms,
        info.defines,
        brightness,
        gamma,
        lightgamma);

    SetConstantBufferBindings(info.name, *pprogram);
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
    if (s_recompileQueued || forceRecompile)
    {
        s_recompileQueued = false;

        double start = g_engfuncs.GetAbsoluteTime();
        g_engfuncs.Con_Printf("WARNING: shader recompile triggered\n");

        for (int i = 0; i < s_shaderCount; i++)
        {
            LoadShaderFromInfo(s_shaders[i], s_brightness, s_gamma, s_lightgamma);
        }

        double end = g_engfuncs.GetAbsoluteTime();
        g_engfuncs.Con_Printf("WARNING: shader recompile took %g ms\n", (end - start) * 1000.0);
    }
}

void shaderUpdateGamma(float brightness, float gamma, float lightgamma)
{
    if (s_brightness != brightness || s_gamma != gamma || s_lightgamma != lightgamma)
    {
        s_brightness = brightness;
        s_gamma = gamma;
        s_lightgamma = lightgamma;
        s_recompileQueued = true;
    }
}

void shaderRegister(const char *name,
    byte *shaderStruct,
    const VertexAttrib *attributes,
    const ShaderUniform *uniforms,
    const char *defines)
{
    GL3_ASSERT(s_shaderCount < MaxShaderInfo);
    ShaderInfo &shaderInfo = s_shaders[s_shaderCount++];
    shaderInfo.name = name;
    shaderInfo.shaderStruct = shaderStruct;
    shaderInfo.attributes = attributes;
    shaderInfo.uniforms = uniforms;
    shaderInfo.defines = defines;
}

}
