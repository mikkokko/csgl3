#include "stdafx.h"
#include "texture.h"
#include "gamma.h"
#include "stb_image.h"

namespace Render
{

constexpr int MaxTextures = 256;

struct Texture
{
    char name[64];
    GLenum target;
    GLuint texture;
};

struct TextureMode
{
    const char *name;
    int min, mag;
};

static cvar_t *gl_texturemode;
static char s_texturemodeString[32];

// defaults not used but set just in case
// actually defaults are used with old enough engines...
static int s_minFilter = GL_LINEAR_MIPMAP_LINEAR;
static int s_magFilter = GL_LINEAR;

static int s_textureCount;
static Texture s_textures[MaxTextures];

static const TextureMode s_textureModes[] = {
    { "GL_NEAREST", GL_NEAREST, GL_NEAREST },
    { "GL_LINEAR", GL_LINEAR, GL_LINEAR },
    { "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
    { "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
    { "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
    { "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

static int MiplessFilter(int filter)
{
    switch (filter)
    {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
        return GL_NEAREST;

    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        return GL_LINEAR;
    }

    return filter;
}

static bool UpdateTextureFilters()
{
    for (const TextureMode &mode : s_textureModes)
    {
        if (!Q_strcasecmp(mode.name, s_texturemodeString))
        {
            s_minFilter = mode.min;
            s_magFilter = mode.mag;
            return true;
        }
    }

    return false;
}

void textureInit()
{
    gl_texturemode = g_engfuncs.pfnGetCvarPointer("gl_texturemode");
}

void textureUpdate()
{
    if (!gl_texturemode)
    {
        // can't know this
        return;
    }

    if (!strcmp(s_texturemodeString, gl_texturemode->string))
    {
        return;
    }

    Q_strcpy_truncate(s_texturemodeString, gl_texturemode->string);

    if (!UpdateTextureFilters())
    {
        // bad cvar value i guess
        GL3_ASSERT(false);
        return;
    }

    // update all of the textures
    for (Texture &texture : s_textures)
    {
        glBindTexture(GL_TEXTURE_2D, texture.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s_minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s_magFilter);
    }
}

void textureGenTextures(GLsizei n, GLuint *textures)
{
    static GLuint current = 55555;

    for (GLsizei i = 0; i < n; i++)
    {
        GL3_ASSERT(!glIsTexture(current));
        textures[i] = current++;
    }
}

GLuint textureFind(GLenum target, const char *name)
{
    for (int i = 0; i < s_textureCount; i++)
    {
        const Texture &texture = s_textures[i];
        if (texture.target == target && !strcmp(texture.name, name))
        {
            return texture.texture;
        }
    }

    return 0;
}

GLuint textureAllocateAndBind(GLenum target, const char *name, bool mipmapped)
{
    GL3_ASSERT(textureFind(target, name) == 0);

    if (s_textureCount >= Q_countof(s_textures))
    {
        platformError("Texture cache full");
    }

    Texture &texture = s_textures[s_textureCount++];
    if (!Q_strcpy_truncate(texture.name, name))
    {
        GL3_ASSERT(false);
        return 0;
    }

    texture.target = target;
    textureGenTextures(1, &texture.texture);

    // bind it and set the texture mode
    glBindTexture(target, texture.texture);

    // FIXME: this sucks
    if (mipmapped)
    {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, s_minFilter);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, s_magFilter);
    }
    else
    {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, MiplessFilter(s_minFilter));
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, MiplessFilter(s_magFilter));
    }

    return texture.texture;
}

GLuint textureLoad2D(const char *path, bool mipmapped, bool gamma)
{
    int fileSize;
    byte *file = g_engfuncs.COM_LoadFile(const_cast<char *>(path), 5, &fileSize);
    if (!file)
    {
        return 0;
    }

    int width, height, comp;
    byte *data = stbi_load_from_memory(file, fileSize, &width, &height, &comp, 4);
    if (!data)
    {
        return 0;
    }

    if (gamma)
    {
        int s = width * height * 4;

        for (int i = 0; i < s; i += 4)
        {
            data[i + 0] = g_gammaTextureTable[data[i + 0]];
            data[i + 1] = g_gammaTextureTable[data[i + 1]];
            data[i + 2] = g_gammaTextureTable[data[i + 2]];
        }
    }

    GLuint texture = textureAllocateAndBind(GL_TEXTURE_2D, path, mipmapped);

    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, mipmapped ? GL_TRUE : GL_FALSE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return texture;
}

}
