#include "stdafx.h"
#include "skybox.h"
#include "gamma.h"
#include "commandbuffer.h"
#include "texture.h"
#include "brush.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Render
{

class SkyShader final : public BaseShader
{
public:
    const char *Name()
    {
        return "sky";
    }

    const VertexAttrib *VertexAttribs()
    {
        return g_brushVertexFormat.attribs;
    }

    const ShaderUniform *Uniforms()
    {
        static const ShaderUniform uniforms[] = {
            SHADER_UNIFORM_CONST(u_texture, 0),
            SHADER_UNIFORM_TERM()
        };

        return uniforms;
    }
};

static char s_skyboxName[32];
static GLuint s_texture;
static SkyShader s_shader;
static bool s_skyboxLoaded;

static const char s_faceExtensions[][3] = {
    "ft",
    "bk",
    "up",
    "dn",
    "rt",
    "lf"
};

void skyboxInit()
{
    // create the texture with textureAllocate so it follows gl_texturemode
    {
        const char textureTag[] = "@@@@@@@@@skybox";
        s_texture = textureAllocateAndBind(GL_TEXTURE_CUBE_MAP, textureTag, false);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    shaderRegister(s_shader);
}

static bool LoadFace(const char *skyboxName, int faceIndex, int &wishWidth, int &wishHeight)
{
    char path[64]; // large enough
    Q_sprintf(path, "gfx/env/%s%s.tga", skyboxName, s_faceExtensions[faceIndex]);

    int fileSize;
    byte *fileData = g_engfuncs.COM_LoadFile(path, 5, &fileSize);
    if (!fileData)
    {
        g_engfuncs.Con_Printf("Could not open file %s\n", path);
        return false;
    }

    int width, height, numComponents;
    byte *data = stbi_load_from_memory(fileData, fileSize, &width, &height, &numComponents, 4);
    g_engfuncs.COM_FreeFile(fileData);

    if (!data)
    {
        g_engfuncs.Con_Printf("Could not load image %s\n", path);
        return false;
    }

    GL3_ASSERT((wishWidth == -1) == (wishHeight == -1));

    if (wishWidth == -1)
    {
        wishWidth = width;
        wishHeight = height;
    }
    else if (width != wishWidth || height != wishHeight)
    {
        g_engfuncs.Con_Printf("Unexpected size for %s (%dx%d, expecting %dx%d)\n", path, width, height, wishWidth, wishHeight);
        stbi_image_free(data);
        return false;
    }

    int size = width * height * 4;

    // this destroys the alpha channel but that's not an issue, it's not used
    for (int j = 0; j < size; j++)
    {
        data[j] = g_gammaTextureTable[data[j]];
    }

    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    return true;
}

static bool LoadSkybox(const char *skyboxName)
{
    // these get set when the first face is loaded, and enforced for all the other faces
    int width = -1, height = -1;

    for (int i = 0; i < 6; i++)
    {
        if (!LoadFace(skyboxName, i, width, height))
        {
            // loading failed, skip the rest
            return false;
        }
    }

    return true;
}

void skyboxUpdate(const char (&skyboxName)[32])
{
    if (!strcmp(s_skyboxName, skyboxName))
    {
        // no change
        return;
    }

    Q_strcpy(s_skyboxName, skyboxName);

    glBindTexture(GL_TEXTURE_CUBE_MAP, s_texture);

    s_skyboxLoaded = LoadSkybox(skyboxName);
    if (!s_skyboxLoaded)
    {
        // desert fallback
        s_skyboxLoaded = LoadSkybox("desert");
    }
}

bool skyboxDrawBegin()
{
    if (!s_skyboxLoaded)
    {
        return false;
    }

    commandUseProgram(&s_shader);

    commandBindTexture(0, GL_TEXTURE_CUBE_MAP, s_texture);

    commandDepthFunc(GL_LEQUAL);

    return true;
}

void skyboxDrawEnd()
{
    GL3_ASSERT(s_skyboxLoaded);
    commandDepthFunc(GL_LESS);
}

}
