#include "stdafx.h"
#include "detail.h"
#include "brush.h"
#include "commandbuffer.h"
#include "memory.h"
#include "texture.h"

namespace Render
{

struct DetailTexture
{
    GLuint name;
    Vector2 scale;
};

enum DetailTextureStatus
{
    DetailNotLoaded,
    DetailLoadFailed,
    DetailLoaded
};

static DetailTextureStatus s_status;
static DetailTexture *s_detailTextures;

static cvar_t *r_detailtextures;

void detailInit()
{
    r_detailtextures = g_engfuncs.pfnGetCvarPointer("r_detailtextures");
}

// lifted from coop, but modified so that it operates on nul terminated strings
// ParseToken will rape the input buffer to insert the nul terminator
class TokenParser
{
public:
    TokenParser(char *buffer)
        : m_current{ buffer }
    {
    }

    [[nodiscard]] const char *ParseToken()
    {
        SkipWhitespace();

        if (!*m_current)
        {
            // nothing to parse
            return nullptr;
        }

        char *start = m_current;
        if (*m_current == '"')
        {
            m_current++;
            start++;

            // FIXME: check unmatched quotes
            while (*m_current && *m_current != '"')
            {
                m_current++;
            }
        }
        else
        {
            while (*m_current && !IsSpace(*m_current))
            {
                m_current++;
            }
        }

        if (*m_current)
        {
            *m_current = '\0';
            m_current++;
        }

        return start;
    }

private:
    void SkipWhitespace()
    {
        while (true)
        {
            while (*m_current && IsSpace(*m_current))
            {
                m_current++;
            }

            if (m_current[0] == '/' && m_current[1] == '/')
            {
                m_current += 2;

                while (*m_current && !IsNewline(*m_current))
                {
                    m_current++;
                }

                continue;
            }

            break;
        }
    }

    static bool IsSpace(char ch)
    {
        unsigned value = ch;
        return (value == ' ') || (value - '\t' < 5);
    }

    static bool IsNewline(char ch)
    {
        return (ch == '\n') || (ch == '\r');
    }

    char *m_current;
};

static bool LoadDetailTexture(const char *name, const char *detailName, float xscale, float yscale)
{
    int i;

    for (i = 0; i < g_worldmodel->numtextures; i++)
    {
        gl3_texture_t &texture = g_worldmodel->textures[i];
        if (!Q_strcasecmp(texture.name, name))
        {
            break;
        }
    }

    if (i == g_worldmodel->numtextures)
    {
        g_engfuncs.Con_Printf("No texture named %s in level\n", name);
        return false;
    }

    char detailPath[4096];
    snprintf(detailPath, sizeof(detailPath), "gfx/%s.tga", detailName);

    DetailTexture &detail = s_detailTextures[i];
    detail.name = textureFind(GL_TEXTURE_2D, detailPath);
    if (!detail.name)
    {
        detail.name = textureLoad2D(detailPath, true, false);
        if (!detail.name)
        {
            g_engfuncs.Con_Printf("Could not load texture %s\n", detailPath);
            return false;
        }
    }

    GL3_ASSERT(xscale == 1.0f || xscale == 2.0f);
    GL3_ASSERT(yscale == 1.0f || yscale == 2.0f);

    detail.scale.x = xscale;
    detail.scale.y = yscale;

    return true;
}

static bool LoadDetailTextureFile()
{
    char mapPath[64];
    Q_strcpy(mapPath, g_worldmodel->engine_model->name);

    char *end = strrchr(mapPath, '.');
    if (!end)
    {
        return false;
    }

    *end = '\0';

    char path[128];
    Q_sprintf(path, "%s_detail.txt", mapPath);

    int fileSize;
    char *fileData = (char *)g_engfuncs.COM_LoadFile(path, 5, &fileSize);
    if (!fileData)
    {
        return false;
    }

    bool success = false;
    bool loadedOne = false;
    TokenParser parser{ fileData };

    // allocate this now i guess
    s_detailTextures = memoryLevelAlloc<DetailTexture>(g_worldmodel->numtextures);

    while (true)
    {
        const char *name = parser.ParseToken();
        if (!name)
        {
            // done parsing
            success = true;
            break;
        }

        const char *detailName = parser.ParseToken();
        const char *xscaleString = parser.ParseToken();
        const char *yscaleString = parser.ParseToken();
        if (!detailName || !xscaleString || !yscaleString)
        {
            g_engfuncs.Con_Printf("%s: unexpected end of file\n", path);
            break;
        }

        // FIXME: properly validate these...
        float xscale = strtof(xscaleString, nullptr);
        float yscale = strtof(yscaleString, nullptr);
        if (xscale <= 0.0f || yscale <= 0.0f)
        {
            g_engfuncs.Con_Printf("%s: invalid scale for texture %s\n", path, name);
            break;
        }

        loadedOne |= LoadDetailTexture(name, detailName, xscale, yscale);
    }

    g_engfuncs.COM_FreeFile(fileData);
    return success && loadedOne;
}

void detailInvalidate()
{
    s_status = DetailNotLoaded;
}

bool detailIsActive()
{
    if (!r_detailtextures || !r_detailtextures->value)
    {
        return false;
    }

    if (s_status == DetailNotLoaded)
    {
        // lazy on demand load
        if (LoadDetailTextureFile())
        {
            s_status = DetailLoaded;
        }
        else
        {
            s_status = DetailLoadFailed;
        }
    }

    return (s_status == DetailLoaded);
}

Vector2 detailSetTextureAndGetScale(int textureIndex)
{
    if (!r_detailtextures || !r_detailtextures->value)
    {
        return {};
    }

    if (s_status == DetailLoaded)
    {
        DetailTexture &texture = s_detailTextures[textureIndex];
        if (!texture.name)
        {
            commandBindTexture(2, GL_TEXTURE_2D, g_state.grayTexture);
            return {};
        }

        commandBindTexture(2, GL_TEXTURE_2D, texture.name);
        return texture.scale;
    }

    return {};
}

void detailClearTexture()
{
    if (!r_detailtextures || !r_detailtextures->value)
    {
        return;
    }

    if (s_status == DetailLoaded)
    {
        commandBindTexture(2, GL_TEXTURE_2D, g_state.grayTexture);
    }
}

}
