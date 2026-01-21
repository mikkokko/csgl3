#include "stdafx.h"
#include "triapigl3.h"
#include "immediate.h"
#include "commandbuffer.h"
#include "pvs.h"
#include "brush.h"
#include "internal.h"
#include "hudgl3.h"
#include "lightstyle.h"

namespace Render
{

struct StudioShadow
{
    int sprite;
    Vector3 p1, p2, p3, p4;
};

triangleapi_t g_triapiGL1;

static cvar_t *gl_fog;

static int s_studioShadowCount;
static StudioShadow s_studioShadows[32];

static int s_rendermode;

static float s_red;
static float s_green;
static float s_blue;
static float s_alpha;

static void RenderMode(int mode)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.RenderMode(mode);
        return;
    }

    s_rendermode = mode;

    switch (mode)
    {
    case kRenderNormal:
        immediateBlendEnable(GL_FALSE);
        immediateDepthMask(GL_TRUE);
        break;

    case kRenderTransColor:
    case kRenderTransTexture:
        immediateBlendEnable(GL_TRUE);
        immediateBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;

    case kRenderTransAlpha:
        immediateBlendEnable(GL_TRUE);
        immediateBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        immediateDepthMask(GL_FALSE);
        break;

    case kRenderTransAdd:
        immediateBlendEnable(GL_TRUE);
        immediateBlendFunc(GL_ONE, GL_ONE);
        immediateDepthMask(GL_FALSE);
        break;

    default:
        GL3_ASSERT(false);
        break;
    }
}

static void Begin(int primitiveCode)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.Begin(primitiveCode);
        return;
    }

    GL3_ASSERT(primitiveCode >= TRI_TRIANGLES && primitiveCode <= TRI_QUAD_STRIP);
    static const int remapMode[] = {
        GL_TRIANGLES,
        GL_TRIANGLE_FAN,
        GL_QUADS,
        GL_POLYGON,
        GL_LINES,
        GL_TRIANGLE_STRIP,
        GL_QUAD_STRIP
    };

    immediateBegin(remapMode[primitiveCode]);
}

static void End(void)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.End();
        return;
    }

    immediateEnd();
}

static void Color4f(float r, float g, float b, float a)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.Color4f(r, g, b, a);
        return;
    }

    // TODO: find everyone associated with condition zero and kill them
    s_red = r;
    s_green = g;
    s_blue = b;
    s_alpha = a;

    if (s_rendermode == kRenderTransAlpha)
    {
        immediateColor4f(r, g, b, a);
    }
    else
    {
        immediateColor4f(r * a, g * a, b * a, 1);
    }
}

static void Color4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.Color4ub(r, g, b, a);
        return;
    }

    s_red = r * (1.0f / 255);
    s_green = g * (1.0f / 255);
    s_blue = b * (1.0f / 255);
    s_alpha = a * (1.0f / 255);

    immediateColor4f(s_red, s_green, s_blue, 1);
}

static void TexCoord2f(float u, float v)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.TexCoord2f(u, v);
        return;
    }

    immediateTexCoord2f(u, v);
}

static void Vertex3fv(float *worldPnt)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.Vertex3fv(worldPnt);
        return;
    }

    immediateVertex3f(worldPnt[0], worldPnt[1], worldPnt[2]);
}

static void Vertex3f(float x, float y, float z)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.Vertex3f(x, y, z);
        return;
    }

    immediateVertex3f(x, y, z);
}

static void Brightness(float brightness)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.Brightness(brightness);
        return;
    }

    immediateColor4f(s_red * s_alpha * brightness, s_green * s_alpha * brightness, s_blue * s_alpha * brightness, 1.0f);
}

static void CullFace(TRICULLSTYLE style)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.CullFace(style);
        return;
    }

    switch (style)
    {
    case TRI_FRONT:
        immediateCullFace(GL_TRUE);
        break;

    case TRI_NONE:
        immediateCullFace(GL_FALSE);
        break;

    default:
        GL3_ASSERT(false);
        break;
    }
}

static int SpriteTexture(struct model_s *pSpriteModel, int frame)
{
    if (!g_state.inFrame)
    {
        return g_triapiGL1.SpriteTexture(pSpriteModel, frame);
    }

    SpriteInfo spriteInfo;
    if (!internalGetSpriteInfo(pSpriteModel, frame, &spriteInfo))
    {
        GL3_ASSERT(false);
        return 0;
    }

    if (!immediateIsActive())
    {
        // NOTE: we also support this when called outside of an immediate frame (used for glowshell texture)
        commandBindTexture(0, GL_TEXTURE_2D, spriteInfo.texture);
        return 1;
    }

    immediateBindTexture(spriteInfo.texture);
    return 1;
}

static int WorldToScreen(float *world, float *screen)
{
    if (!g_state.inFrame) // fixme
    {
        return g_triapiGL1.WorldToScreen(world, screen);
    }

    return hudWorldToScreen(world, screen[0], screen[1]);
}

static void Fog(float flFogColor[3], float flStart, float flEnd, int bOn)
{
    // let the engine know, too
    g_triapiGL1.Fog(flFogColor, flStart, flEnd, bOn);

    g_state.fogEnabled = (bOn && gl_fog->value);
    if (g_state.fogEnabled)
    {
        g_state.fogColor.x = flFogColor[0] * (1.0f / 255);
        g_state.fogColor.y = flFogColor[1] * (1.0f / 255);
        g_state.fogColor.z = flFogColor[2] * (1.0f / 255);
    }
}

static void ScreenToWorld(float *screen, float *world)
{
    if (!g_state.inFrame) // fixme
    {
        g_triapiGL1.ScreenToWorld(screen, world);
        return;
    }

    g_engfuncs.Con_Printf("[%s] stop calling into triapi you dumb fuck\n", __func__);
}

static void GetMatrix(const int pname, float *matrix)
{
    if (!g_state.inFrame) // fixme
    {
        g_triapiGL1.GetMatrix(pname, matrix);
        return;
    }

    switch (pname)
    {
    case GL_MODELVIEW_MATRIX:
        // no model matrix
        memcpy(matrix, &g_state.viewMatrix, sizeof(g_state.viewMatrix));
        break;

    case GL_PROJECTION_MATRIX:
        memcpy(matrix, &g_state.projectionMatrix, sizeof(g_state.projectionMatrix));
        break;

    default:
        GL3_ASSERT(false);
        break;
    }
}

static int BoxInPVS(float *mins, float *maxs)
{
    if (!g_state.active)
    {
        return g_triapiGL1.BoxInPVS(mins, maxs);
    }

    return pvsNode(g_worldmodel->nodes, mins, maxs) ? 1 : 0;
}

static void LightAtPoint(float *pos, float *value)
{
    if (!g_state.active)
    {
        return g_triapiGL1.LightAtPoint(pos, value);
    }

    Vector3 end = pos;
    end.z -= 2048.0f;

    Vector3 result = lightstyleApply(internalSampleLightmap(g_worldmodel->engine_model, pos, end));

    value[0] = result.x;
    value[1] = result.y;
    value[2] = result.z;
}

static void Color4fRendermode(float r, float g, float b, float a, int rendermode)
{
    if (!g_state.inFrame)
    {
        g_triapiGL1.Color4fRendermode(r, g, b, a, rendermode);
        return;
    }

    // no clue what this is, classic czero junk
    if (rendermode == kRenderTransAlpha)
    {
        s_alpha = a * (1.0f / 255);
        immediateColor4f(r, g, b, a);
    }
    else
    {
        immediateColor4f(r * a, g * a, b * a, 1.0f);
    }
}

static void FogParams(float flDensity, int iFogSkybox)
{
    // let the engine know too
    g_triapiGL1.FogParams(flDensity, iFogSkybox);

    g_state.fogDensity = flDensity;
    g_state.fogSkybox = iFogSkybox ? true : false;
}

static void RenderStudioShadows()
{
    if (!s_studioShadowCount)
    {
        return;
    }

    // fixme cleanup
    RenderMode(kRenderTransAlpha);
    Color4f(0, 0, 0, 1);

    for (int i = 0; i < s_studioShadowCount; i++)
    {
        StudioShadow &shadow = s_studioShadows[i];
        model_t *model = g_engfuncs.hudGetModelByIndex(shadow.sprite);
        if (!model)
        {
            //GL3_ASSERT(false);
            continue;
        }

        SpriteTexture(model, 0);

        Begin(TRI_QUADS);
        TexCoord2f(0, 0);
        Vertex3fv(&shadow.p1.x);
        TexCoord2f(0, 1);
        Vertex3fv(&shadow.p2.x);
        TexCoord2f(1, 1);
        Vertex3fv(&shadow.p3.x);
        TexCoord2f(1, 0);
        Vertex3fv(&shadow.p4.x);
        End();
    }

    s_studioShadowCount = 0;
    RenderMode(kRenderNormal);
}

static const triangleapi_t s_triapiGL3 = {
    TRI_API_VERSION,
    RenderMode,
    Begin,
    End,
    Color4f,
    Color4ub,
    TexCoord2f,
    Vertex3fv,
    Vertex3f,
    Brightness,
    CullFace,
    SpriteTexture,
    WorldToScreen,
    Fog,
    ScreenToWorld,
    GetMatrix,
    BoxInPVS,
    LightAtPoint,
    Color4fRendermode,
    FogParams
};

void triapiInit()
{
    gl_fog = g_engfuncs.pfnGetCvarPointer("gl_fog");

    g_triapiGL1 = *g_engfuncs.pTriAPI;
    *g_engfuncs.pTriAPI = s_triapiGL3;
}

void triapiBegin()
{
    immediateDrawStart(false);

    // now this is a "bruh moment" if 've even seen one
    RenderStudioShadows();
}

void triapiEnd()
{
    immediateDrawEnd();
}

void triapiQueueStudioShadow(int sprite, float *p1, float *p2, float *p3, float *p4)
{
    if (s_studioShadowCount >= Q_countof(s_studioShadows))
    {
        GL3_ASSERT(false);
        return;
    }

    StudioShadow &shadow = s_studioShadows[s_studioShadowCount++];
    shadow.sprite = sprite;
    shadow.p1 = p1;
    shadow.p2 = p2;
    shadow.p3 = p3;
    shadow.p4 = p4;
}

}
