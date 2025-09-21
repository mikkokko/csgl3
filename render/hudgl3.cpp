#include "stdafx.h"
#include "hudgl3.h"
#include "effects.h"

namespace Render
{

static int s_sprite;
static wrect_t s_rect;
static int s_red;
static int s_green;
static int s_blue;

static void (*pfnSetCrosshair)(int, wrect_t, int, int, int);

static void Hk_SetCrosshair(int hspr, wrect_t rc, int r, int g, int b)
{
    s_sprite = hspr;
    s_rect = rc;
    s_red = r;
    s_green = g;
    s_blue = b;

    pfnSetCrosshair(hspr, rc, r, g, b);
}

void hudInit(cl_enginefunc_t *engfuncs)
{
    pfnSetCrosshair = engfuncs->pfnSetCrosshair;
    engfuncs->pfnSetCrosshair = Hk_SetCrosshair;
}

bool hudWorldToScreen(const Vector3 &p, float &x, float &y)
{
    const Matrix4 &m = g_state.viewProjectionMatrix;

    float clip_x = m.m00 * p.x + m.m10 * p.y + m.m20 * p.z + m.m30;
    float clip_y = m.m01 * p.x + m.m11 * p.y + m.m21 * p.z + m.m31;
    float clip_w = m.m03 * p.x + m.m13 * p.y + m.m23 * p.z + m.m33;

    x = clip_x / clip_w;
    y = clip_y / clip_w;

    // return value is used by triapi
    return clip_w <= 0;
}

#ifdef SCHIZO_DEBUG
static const char *PrettySize(int bytes)
{
    static char buffer[32];

    if (bytes < 1024)
    {
        Q_sprintf(buffer, "%d B", bytes);
    }
    else if (bytes < 1024 * 1024)
    {
        Q_sprintf(buffer, "%d KB", (int)(bytes / 1024.0));
    }
    else
    {
        Q_sprintf(buffer, "%d MB", (int)(bytes / (1024.0 * 1024.0)));
    }

    return buffer;
}

static void DrawRenderHud(int screenWidth)
{
    static int s_frameCount;
    static double s_lastTime;
    static int s_lastFps;

    s_frameCount++;
    double time = g_engfuncs.GetAbsoluteTime();

    if (time - s_lastTime >= 1.0)
    {
        s_lastTime = time;

        s_lastFps = s_frameCount;
        s_frameCount = 0;
    }

    int red = g_state.active ? 0 : 255;
    int green = g_state.active ? 255 : 0;
    int blue = 0;

    char string[256];
    Q_sprintf(string, "[%s] %d FPS", g_state.active ? "GL3" : "GL1", s_lastFps);
    g_engfuncs.pfnDrawString(screenWidth - 256, 64, string, red, green, blue);

    if (g_state.active)
    {
        int yoffset = 80;

        Q_sprintf(string, "Vertex buffer size %s", PrettySize(g_state.vertexBufferSize));
        g_engfuncs.pfnDrawString(screenWidth - 256, yoffset, string, red, green, blue);
        yoffset += 16;

        Q_sprintf(string, "Index buffer size %s", PrettySize(g_state.indexBufferSize));
        g_engfuncs.pfnDrawString(screenWidth - 256, yoffset, string, red, green, blue);
        yoffset += 16;

        Q_sprintf(string, "Uniform buffer size %s", PrettySize(g_state.uniformBufferSize));
        g_engfuncs.pfnDrawString(screenWidth - 256, yoffset, string, red, green, blue);
        yoffset += 16;

        Q_sprintf(string, "Draw call count %d", g_state.drawcallCount);
        g_engfuncs.pfnDrawString(screenWidth - 256, yoffset, string, red, green, blue);
        yoffset += 16;

        Q_sprintf(string, "Command buffer size %d", g_state.commandBufferSize);
        g_engfuncs.pfnDrawString(screenWidth - 256, yoffset, string, red, green, blue);
    }
}
#endif

void PreDrawHud()
{
    if (!g_state.active)
        return;

    effectsBeginDlightCapture();
}

void PostDrawHud(int screenWidth, int screenHeight)
{
#ifdef SCHIZO_DEBUG
    DrawRenderHud(screenWidth);
 #endif

    if (!g_state.active)
    {
        return;
    }

    effectsEndDlightCapture();

    // NOTE: on 1.6, we always want to draw the crosshair
    if (s_sprite)
    {
        // on 1.6 we always want to center the crosshair to avoid jitter
        int x = screenWidth / 2;
        int y = screenHeight / 2;

        int yPos = static_cast<int>(y) - (s_rect.bottom - s_rect.top) / 2;
        int xPos = static_cast<int>(x) - (s_rect.right - s_rect.left) / 2;

        g_engfuncs.pfnSPR_Set(s_sprite, s_red, s_green, s_blue);
        g_engfuncs.pfnSPR_DrawHoles(0, xPos, yPos, &s_rect);
    }

    // disable engine crosshair
    g_engfuncs.Cvar_Set("crosshair", "0");
}

}
