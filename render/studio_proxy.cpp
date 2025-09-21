#include "stdafx.h"
#include "studio_proxy.h"
#include "studio_cache.h"
#include "studio_misc.h"
#include "studio_render.h"
#include "triapigl3.h"

namespace Render
{

engine_studio_api_t orig_studio;

#define NOT_IMPL() \
    do \
    { \
        static bool bitched; \
        if (!bitched) \
            g_engfuncs.Con_Printf("%s\n", __FUNCTION__); \
        bitched = true; \
    } while (0)

// keep track of the model currently being rendered
static model_t *s_model;
static studiohdr_t *s_header;

// last model passed to Mod_Extradata
static model_t *last_model;

static StudioContext s_context;

static void *Hk_Mod_Extradata(model_t *model)
{
    last_model = model;
    return orig_studio.Mod_Extradata(model);
}

static void Hk_GetTimes(int *framecount, double *current, double *old)
{
    orig_studio.GetTimes(framecount, current, old);

    // need to use our own framecount
    if (g_state.active)
    {
        *framecount = g_state.frameCount;
    }
}

static model_t *DeduceModel(studiohdr_t *header)
{
    GL3_ASSERT(header);

    if (last_model->cache.data == header)
        return last_model;

    // shouldn't happen
    GL3_ASSERT(false);
    return nullptr;
}

static void StudioSetupModel(int bodypart, void **ppbodypart, void **ppsubmodel)
{
    if (!g_state.active)
        orig_studio.StudioSetupModel(bodypart, ppbodypart, ppsubmodel);
    else
    {
        studioSetupModel(s_context, bodypart,
            reinterpret_cast<mstudiobodyparts_t **>(ppbodypart),
            reinterpret_cast<mstudiomodel_t **>(ppsubmodel));
    }
}

static int StudioCheckBBox()
{
    if (!g_state.active)
        return orig_studio.StudioCheckBBox();
    else
    {
        GL3_ASSERT(s_header);
        return !studioFrustumCull(s_context.entity, s_header);
    }
}

static void StudioDynamicLight(struct cl_entity_s *ent, struct alight_s *plight)
{
    if (!g_state.active)
        orig_studio.StudioDynamicLight(ent, plight);
    else
    {
        studioDynamicLight(ent, plight);
    }
}

static void StudioEntityLight(struct alight_s *plight)
{
    if (!g_state.active)
        orig_studio.StudioEntityLight(plight);
    else
    {
        studioEntityLight(s_context);
    }
}

static void StudioSetupLighting(struct alight_s *plighting)
{
    if (!g_state.active)
        orig_studio.StudioSetupLighting(plighting);
    else
    {
        if (!s_model || (s_model->cache.data && (s_model->cache.data != s_header)))
        {
            // this happens with player weapon models... Mod_Extradata is called right
            // before this so we should be able to get the model pointer from there
            s_model = DeduceModel(s_header);
        }

        GL3_ASSERT(s_model);
        GL3_ASSERT(s_header);

        // we now have enough information to set these
        s_context.model = s_model;
        s_context.header = s_header;
        s_context.cache = studioCacheGet(s_model, s_header);

        studioSetupLighting(s_context, plighting);
    }
}

static void StudioDrawPoints()
{
    if (!g_state.active)
        orig_studio.StudioDrawPoints();
    else
    {
        studioDrawPoints(s_context);
    }
}

static void StudioDrawHulls()
{
    if (!g_state.active)
        orig_studio.StudioDrawHulls();
    else
    {
        NOT_IMPL();
    }
}

static void StudioDrawAbsBBox()
{
    if (!g_state.active)
        orig_studio.StudioDrawAbsBBox();
    else
    {
        NOT_IMPL();
    }
}

static void StudioDrawBones()
{
    if (!g_state.active)
        orig_studio.StudioDrawBones();
    else
    {
        NOT_IMPL();
    }
}

static void StudioSetupSkin(void *ptexturehdr, int index)
{
    if (!g_state.active)
        orig_studio.StudioSetupSkin(ptexturehdr, index);
    else
    {
        NOT_IMPL();
    }
}

static void StudioSetRemapColors(int top, int bottom)
{
    if (!g_state.active)
        orig_studio.StudioSetRemapColors(top, bottom);
    else
    {
        NOT_IMPL();
    }
}

static model_t *SetupPlayerModel(int index)
{
    if (!g_state.active)
        return orig_studio.SetupPlayerModel(index);
    else
    {
        // fixme remove
        return orig_studio.SetupPlayerModel(index);
    }
}

static void StudioSetHeader(void *header)
{
    // the engine needs this (R_StudioClientEvents for example)
    orig_studio.StudioSetHeader(header);

    if (g_state.active)
    {
        s_header = static_cast<studiohdr_t *>(header);
    }
}

static void SetRenderModel(model_t *model)
{
    if (!g_state.active)
        orig_studio.SetRenderModel(model);
    else
    {
        s_model = model;
    }
}

static void SetupRenderer(int rendermode)
{
    if (!g_state.active)
        orig_studio.SetupRenderer(rendermode);
    else
    {
        studioSetupRenderer(s_context, rendermode);
    }
}

static void RestoreRenderer()
{
    if (!g_state.active)
        orig_studio.RestoreRenderer();
    else
    {
        studioRestoreRenderer(s_context);
    }
}

static void SetChromeOrigin()
{
    if (!g_state.active)
        orig_studio.SetChromeOrigin();
    else
    {
        // do nothing, we set the chrome origin in studioSetupRenderer
    }
}

static void GL_StudioDrawShadow()
{
    if (!g_state.active)
        orig_studio.GL_StudioDrawShadow();
    else
    {
        NOT_IMPL();
    }
}

static void GL_SetRenderMode(int mode)
{
    if (!g_state.active)
        orig_studio.GL_SetRenderMode(mode);
    else
    {
        // we don't have per mesh render modes, so do
        // all setup in studioSetupRenderer
    }
}

static void StudioSetRenderamt(int iRenderamt)
{
    if (!g_state.active)
        orig_studio.StudioSetRenderamt(iRenderamt);
    else
    {
        NOT_IMPL();
    }
}

static void StudioSetCullState(int iCull)
{
    if (!g_state.active)
        orig_studio.StudioSetCullState(iCull);
    else
    {
        NOT_IMPL();
    }
}

static void StudioRenderShadow(int iSprite, float *p1, float *p2, float *p3, float *p4)
{
    if (!g_state.active)
        orig_studio.StudioRenderShadow(iSprite, p1, p2, p3, p4);
    else
    {
        triapiQueueStudioShadow(iSprite, p1, p2, p3, p4);
    }
}

static void SetupStudioProxy(struct engine_studio_api_s *studio)
{
    orig_studio = *studio;

    studio->Mod_Extradata = Hk_Mod_Extradata;
    studio->GetTimes = Hk_GetTimes;
    studio->StudioSetupModel = StudioSetupModel;
    studio->StudioCheckBBox = StudioCheckBBox;
    studio->StudioDynamicLight = StudioDynamicLight;
    studio->StudioEntityLight = StudioEntityLight;
    studio->StudioSetupLighting = StudioSetupLighting;
    studio->StudioDrawPoints = StudioDrawPoints;
    studio->StudioDrawHulls = StudioDrawHulls;
    studio->StudioDrawAbsBBox = StudioDrawAbsBBox;
    studio->StudioDrawBones = StudioDrawBones;
    studio->StudioSetupSkin = StudioSetupSkin;
    studio->StudioSetRemapColors = StudioSetRemapColors;
    studio->SetupPlayerModel = SetupPlayerModel;
    studio->StudioSetHeader = StudioSetHeader;
    studio->SetRenderModel = SetRenderModel;
    studio->SetupRenderer = SetupRenderer;
    studio->RestoreRenderer = RestoreRenderer;
    studio->SetChromeOrigin = SetChromeOrigin;
    studio->GL_StudioDrawShadow = GL_StudioDrawShadow;
    studio->GL_SetRenderMode = GL_SetRenderMode;
    studio->StudioSetRenderamt = StudioSetRenderamt;
    studio->StudioSetCullState = StudioSetCullState;
    studio->StudioRenderShadow = StudioRenderShadow;
}

void studioProxyInit(struct engine_studio_api_s *studio)
{
    studioRenderInit();
    SetupStudioProxy(studio);
}

void studioProxyDrawEntity(int flags, cl_entity_t *entity, float blend)
{
    // studio model rendering still calls into engine code that
    // expects the currententity global to be set
    platformSetCurrentEntity(entity);

    memset(&s_context, 0, sizeof(s_context));

    s_context.entity = entity;
    s_context.blend = blend;

    GL3_ASSERT(entity->curstate.movetype != MOVETYPE_FOLLOW);
    GL3_ASSERT(!entity->curstate.aiment);

    if (entity->player)
    {
        (*g_pstudio)->StudioDrawPlayer(flags, g_engineStudio.GetPlayerState(entity->index - 1));
    }
    else
    {
        (*g_pstudio)->StudioDrawModel(flags);
    }
}

}
