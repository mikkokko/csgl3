#include "stdafx.h"
#include "decal.h"
#include "gamma.h"
#include "immediate.h"
#include "skybox.h"
#include "water.h"
#include "sprite.h"
#include "studio_cache.h"
#include "lightstyle.h"
#include "commandbuffer.h"
#include "dynamicbuffer.h"
#include "texture.h"
#include "memory.h"
#include "brush.h"
#include "internal.h"
#include "studio_proxy.h"
#include "entity.h"
#include "effects.h"
#include "triapigl3.h"
#include "hudgl3.h"
#include "screenfadegl3.h"
#include "particle.h"
#include "studio_misc.h"
#include "beam.h"

extern "C" void HUD_DrawNormalTriangles();
extern "C" void HUD_DrawTransparentTriangles();

namespace Render
{

struct SceneParams
{
    Vector3 origin;
    Vector3 angles;
    Vector3 forward, right, up;

    float fov;
    float viewModelFov;

    int viewport_x;
    int viewport_y;
    int viewport_w;
    int viewport_h;
};

// must match the shader
struct FrameConstants
{
    Matrix4 viewProjectionMatrix;
    Matrix4 skyMatrix; // sky is rendered every frame so leave this here for now
    Matrix4 vmViewProjectionMatrix; // viewmodel is rendered every frame so leave this here for now
    Vector4 cameraRight; // only used by studio model chrome? why have it here

    Vector4 clientTime; // FIXME: could pack with... something

    Vector4 lightPositions[MAX_SHADER_LIGHTS]; // w stores 1/radius
    Vector4 lightColors[MAX_SHADER_LIGHTS];

    // accessed with lightstyles[i].x
    Vector4 lightstyles[MAX_LIGHTSTYLES];
};

// must match the shader
struct FogConstants
{
    // rgb color
    Vector4 fogColor;

    // x: -density*density*log2(e)
    // y: skybox fog factor
    Vector4 fogParams;
};

cl_enginefunc_t g_engfuncs;
r_studio_interface_t **g_pstudio;
engine_studio_api_t g_engineStudio;

State g_state;

cvar_t *r_fullbright;
cvar_t *v_direct;
cvar_t *gl_spriteblend;

dlight_t *g_dlights;
dlight_t *g_elights;

// nullptr on initalization failure
static cvar_t *gl3_enable;

// set r_norefresh to 1 every frame to disable most of engine's rendering
static cvar_t *r_norefresh;

// 25th anniversary update widescreen fov
static cvar_t *gl_widescreen_yfov;

// need to do this due to our dynamic buffer system...
// upload fog and non-fog constant buffers, switch between them with
// VSSetConstantBuffers if we want to disable fog for some of the objects
static bool s_fogConstantsEnabled;
BufferSpan s_fogConstants[2];

static int Hk_CreateVisibleEntity(int type, cl_entity_t *entity)
{
    if (AddEntity(type, entity))
    {
        // was added by the renderer
        // return value might be wrong but who gives a shit
        return 1;
    }

    // let the engine have a whack at it
    return g_engfuncs.CL_CreateVisibleEntity(type, entity);
}

void ModifyEngfuncs(cl_enginefunc_t *engfuncs)
{
    // save these off so we can call them later
    g_engfuncs = *engfuncs;

    // hook in case the game uses this
    engfuncs->CL_CreateVisibleEntity = Hk_CreateVisibleEntity;

    // need to render crosshair ourselves, this will hook pfnSetCrosshair
    hudInit(engfuncs);
}

static cvar_t *UnregisterCvar(const char *name)
{
    // g_engfuncs.GetFirstCvarPtr might not exist, so fuck up the name instead
    cvar_t *cvar = g_engfuncs.pfnGetCvarPointer(name);
    if (cvar)
    {
        cvar->name = "@unregistered";
        return cvar;
    }

    return NULL;
}

static void SetupState()
{
    // we're not using vertex array objects, vgui2 enables these so disable
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glDepthFunc(GL_LESS);

    // should already be the case
    GL3_ASSERT(glIsEnabled(GL_CULL_FACE));

    // need to disable these
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
}

static void RestoreState()
{
    glUseProgram(0);

    // we didn't use vertex array objects.. disable all attribs we used to not break vgui2
    for (int i = 0; i < MaxVertexAttribs; i++)
    {
        glDisableVertexAttribArray(i);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    //glBindBuffer(GL_UNIFORM_BUFFER, 0); // not needed

    // why is this needed.... buggy intel drivers or am i retarded?
    // update: the latter (also happens on amd), but haven't investigated why
#if 1
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
#endif

    // we've changed the bound texture so
    // reset it to 0 via engine's GL_Bind
    internalClearBoundTexture();
}

#ifdef GL_ERROR_CHECK
void APIENTRY MessageCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar *message,
    const void *)
{
    g_engfuncs.Con_Printf("[THE GL] [%X] [%X] [%X] %s\n", type, id, severity, message);
}

static void DebugInit()
{
    if (!GLAD_GL_KHR_debug)
    {
        return;
    }

    // engine renderer errors will flood the console
    // but maybe it's still useful
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, nullptr);
}
#endif

void Initialize(struct engine_studio_api_s *studio, r_studio_interface_s **pinterface)
{
    g_engineStudio = *studio;
    g_pstudio = pinterface;

    if (studio->IsHardware() != 1)
    {
        platformError("Running in Software or D3D mode, only OpenGL is supported");
    }

    if (!gladLoadGL())
    {
        platformError("Could not load OpenGL functions");
    }

    if (!GLAD_GL_VERSION_3_1)
    {
        platformError("OpenGL 3.1 required, current context is %d.%d", GLVersion.major, GLVersion.minor);
    }

    if (!GLAD_GL_ARB_draw_elements_base_vertex)
    {
        platformError("ARB_draw_elements_base_vertex required");
    }

    // calls platformError on failure
    platformInit(
        reinterpret_cast<void *>(studio->GetViewInfo),
        reinterpret_cast<void *>(studio->GetCurrentEntity),
        studio->GetViewEntity());

    // unregister r_norefresh
    r_norefresh = UnregisterCvar("r_norefresh");
    if (!r_norefresh)
    {
        platformError("No such cvar r_norefresh, can't disable engine rendering");
    }

    // toggling between gl1 and gl3 is buggy, but allow it anyway
    gl3_enable = g_engfuncs.pfnRegisterVariable("gl3_enable", "1", 0);

    // might not exist
    gl_widescreen_yfov = g_engfuncs.pfnGetCvarPointer("gl_widescreen_yfov");

    // other cvars...
    r_fullbright = g_engfuncs.pfnGetCvarPointer("r_fullbright");
    v_direct = g_engfuncs.pfnGetCvarPointer("direct");
    gl_spriteblend = g_engfuncs.pfnGetCvarPointer("gl_spriteblend");

#ifdef GL_ERROR_CHECK
    DebugInit();
#endif

    memoryInit();
    gammaInit();
    shaderInit();
    immediateInit();
    textureInit();
    brushInit();
    decalInit();
    studioProxyInit(studio);
    skyboxInit();
    waterInit();
    spriteInit();
    effectsInit(g_engfuncs.pEfxAPI, studio);
    commandInit();
    dynamicBuffersInit();
    triapiInit();
    particleInit();
    screenFadeInit();

    // dummy textures for fullbright etc.
    {
        const int width = 1, height = 1;
        byte data[width * height * 4];
        memset(data, 0xff, sizeof(data));

        textureGenTextures(1, &g_state.whiteTexture);
        glBindTexture(GL_TEXTURE_2D, g_state.whiteTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }

    // get pointer to first elight
    g_dlights = g_engfuncs.pEfxAPI->CL_AllocDlight(0);
    g_elights = g_engfuncs.pEfxAPI->CL_AllocElight(0);

    // back to fixed pipeline
    RestoreState();
}

// computes a hash to detect if the level has changed or been reloaded
// looks fucked, but the level name alone isn't reliable as the same level can be reloaded,
// and the memory locations of internal structures (which we point to) may change
// this is why we combine a hash of the model name with a value of a hunk allocated pointer
// (using model->entities as it can't realistically be 0)
static uint64_t ComputeLevelHash(const model_t *model)
{
    if (!model)
    {
        return 0;
    }

    // these are 32-bit values, we combine them to a 64-bit one
    static_assert(sizeof(uintptr_t) == sizeof(uint32_t), "fuck you idiot");
    uint64_t hash1 = HashString(model->name);
    uint64_t hash2 = reinterpret_cast<uintptr_t>(model->entities);

    return hash1 | (hash2 << 32);
}

static void CheckLevelChange()
{
    // probably engine errors???
    GL_ERRORS_QUIET();

    static uint64_t previousHash;

    model_t *worldmodel = g_engineStudio.GetModelByIndex(1);
    uint64_t hash = ComputeLevelHash(worldmodel);
    if (hash == previousHash)
    {
        // no change
        return;
    }

    previousHash = hash;

    // free the previous level data
    brushFreeWorldModel();
    memoryLevelFree();

    // if the level changed, load it to g_worldmodel
    if (worldmodel)
    {
        brushLoadWorldModel(worldmodel);

        // try to prebuild model caches to avoid hitches at draw time
        studioCacheTouchAll();

        // brush model loading and studio cache touching
        // can change opengl state so restore it
        RestoreState();

        // i guess
        particleClear();

        // not sure if needed
        lightstyleReset();
    }
}

int BeginFrame()
{
    // just in case
    gammaUpdate();

    if (!gl3_enable->value)
    {
        g_state.active = false;
        r_norefresh->value = 0;
        return false;
    }

    g_state.active = true;
    r_norefresh->value = 1;

    CheckLevelChange();

    // clear entities from the previous frame
    entityClearBuckets();

    return true;
}

static float VerticalFov(float horizontalFov, float aspectRatioInverse)
{
    if (gl_widescreen_yfov && gl_widescreen_yfov->value)
    {
        aspectRatioInverse = 0.75f;
    }

    return 2.0f * atanf(tanf(horizontalFov * 0.5f) * aspectRatioInverse);
}

// mvp matrix for the skybox, rotated and flipped for cubemap textures
static Matrix4 SkyMatrix(const Vector3 &cameraPosition)
{
    Matrix4 result;

    result.m00 = 0;
    result.m01 = 0;
    result.m02 = 1;
    result.m03 = 0;

    result.m10 = -1;
    result.m11 = 0;
    result.m12 = 0;
    result.m13 = 0;

    result.m20 = 0;
    result.m21 = 1;
    result.m22 = 0;
    result.m23 = 0;

    result.m30 = cameraPosition.y;
    result.m31 = -cameraPosition.z;
    result.m32 = -cameraPosition.x;
    result.m33 = 1;

    return result;
}

static void UpdateFrameConstants(const Matrix4 &vmViewProjectionMatrix)
{
    FrameConstants frameConstants;
    frameConstants.viewProjectionMatrix = g_state.viewProjectionMatrix;
    frameConstants.skyMatrix = SkyMatrix(g_state.viewOrigin);
    frameConstants.vmViewProjectionMatrix = vmViewProjectionMatrix;

    frameConstants.cameraRight = { g_state.viewRight, 1 }; // temp

    static_assert(sizeof(frameConstants.lightstyles) == sizeof(g_lightstyles) * 4, "wtf");
    for (int i = 0; i < MAX_LIGHTSTYLES; i++)
    {
        float value = g_lightstyles[i];
        frameConstants.lightstyles[i] = { value, 0, 0, 0 };
    }

    int numLights = 0;

    for (int i = 0; i < MAX_DLIGHTS; i++)
    {
        dlight_t *light = &g_dlights[i];
        if (light->radius < 0.01f || light->die < g_engfuncs.GetClientTime())
        {
            continue;
        }

        if (g_state.viewFrustum.CullSphere(light->origin, light->radius))
        {
            continue;
        }

        if (numLights == MAX_SHADER_LIGHTS)
        {
            // could do a more aggressive cull???
            break;
        }

        frameConstants.lightPositions[numLights] = { light->origin, 1.0f / light->radius };

        frameConstants.lightColors[numLights].x = static_cast<float>(light->color.r) / 255.0f;
        frameConstants.lightColors[numLights].y = static_cast<float>(light->color.g) / 255.0f;
        frameConstants.lightColors[numLights].z = static_cast<float>(light->color.b) / 255.0f;

        numLights++;
    }

    for (int i = numLights; i < MAX_SHADER_LIGHTS; i++)
    {
        frameConstants.lightPositions[i] = {};
        frameConstants.lightColors[i] = {};
    }

    frameConstants.clientTime.x = g_engfuncs.GetClientTime();

    BufferSpan span = dynamicUniformData(&frameConstants, sizeof(frameConstants));
    commandBindUniformBuffer(0, span.buffer, span.byteOffset, sizeof(frameConstants));
}

void renderFogEnable(bool enable, bool forceUpdate)
{
    if (enable && !g_state.sceneHasFog)
    {
        enable = false;
    }

    if (!forceUpdate && s_fogConstantsEnabled == enable)
    {
        // no change
        return;
    }

    s_fogConstantsEnabled = enable;

    const BufferSpan &constants = s_fogConstants[enable];
    GL3_ASSERT(constants.buffer && constants.data);
    commandBindUniformBuffer(2, constants.buffer, constants.byteOffset, sizeof(FogConstants));
}

static float WaterFogDensity(float linearEnd)
{
    // constant selected by a dice roll
    return 1.665f / linearEnd;
}

static void UpdateFogConstants()
{
    FogConstants noFogConstants;
    noFogConstants.fogColor = { 0.0f, 1.0f, 0.0f, 1.0f };
    noFogConstants.fogParams = { 1.0f, 1.0f, 0.0f, 0.0f };
    s_fogConstants[false] = dynamicUniformData(&noFogConstants, sizeof(FogConstants));

    if (g_state.sceneHasFog)
    {
        Vector3 fogColor;
        float fogDensity;
        bool fogSkybox;

        if (g_state.inWater)
        {
            Color32 temp = g_state.waterColor;
            fogColor = { temp.r / 255.0f, temp.g / 255.0f, temp.b / 255.0f };
            fogDensity = WaterFogDensity(static_cast<float>(4 * (384 - g_state.waterColor.a)));
            fogSkybox = false;
        }
        else
        {
            fogColor = g_state.fogColor;
            fogDensity = g_state.fogDensity;
            fogSkybox = g_state.fogSkybox;
        }

        // fog factor is computed as exp2(param * coord * coord)
        const float log2e = 1.442695040889f; // log2(e)
        float param = -log2e * fogDensity * fogDensity;

        FogConstants fogConstants;
        fogConstants.fogColor = { fogColor, 1.0f };
        fogConstants.fogParams = { param, fogSkybox ? 0.0f : 1.0f, 0.0f, 0.0f };
        s_fogConstants[true] = dynamicUniformData(&fogConstants, sizeof(FogConstants));
    }

    // if the level has fog, we start with fog enabled
    // otherwise we'll go with no fog
    renderFogEnable(g_state.sceneHasFog, true);
}

static void ViewModelProjectionMatrix(Matrix4 &matrix, float fovScale, float aspect)
{
    constexpr float znear = 1.0f;
    constexpr float zfar = 4096.0f;

    float f = 1.0f / fovScale;

    matrix.m00 = f / aspect;
    matrix.m01 = 0;
    matrix.m02 = 0;
    matrix.m03 = 0;

    matrix.m10 = 0;
    matrix.m11 = f;
    matrix.m12 = 0;
    matrix.m13 = 0;

    matrix.m20 = 0;
    matrix.m21 = 0;
    matrix.m22 = ((0.4f * (zfar - 2.5f * znear)) / (zfar - znear));
    matrix.m23 = -1;

    matrix.m30 = 0;
    matrix.m31 = 0;
    matrix.m32 = ((-0.6f * zfar * znear) / (zfar - znear));
    matrix.m33 = 0;
}

static void SetupViewport(const SceneParams &params)
{
    SCREENINFO screenInfo{};
    screenInfo.iSize = sizeof(screenInfo);
    g_engfuncs.pfnGetScreenInfo(&screenInfo);

    int y = screenInfo.iHeight - params.viewport_y - params.viewport_h;
    glViewport(params.viewport_x, y, params.viewport_w, params.viewport_h);

    glClear(GL_DEPTH_BUFFER_BIT);
}

static void SetupView(const SceneParams &params)
{
    Matrix4 viewMatrix = ViewMatrix(params.origin, params.forward, params.right, params.up);

    float aspectRatio = (float)params.viewport_w / params.viewport_h;

    float yFov = VerticalFov(Radians(params.fov), 1.0f / aspectRatio);

    float zNear = 4.0f;
    float zFar = g_state.movevars->zmax;

    Matrix4 projectionMatrix = ProjectionMatrix(yFov, aspectRatio, zNear, zFar);

    Matrix4 viewProjectionMatrix = projectionMatrix * viewMatrix;

    // viewmodel has its own projection matrix for separate fov, depth range and clipping planes
    float vmFovScale = 0.75f * tanf(Radians(params.viewModelFov) * 0.5f);
    Matrix4 vmProjectionMatrix;
    ViewModelProjectionMatrix(vmProjectionMatrix, vmFovScale, aspectRatio);
    Matrix4 vmViewProjectionMatrix = vmProjectionMatrix * viewMatrix;

    // update these mofos
    g_state.viewOrigin = params.origin;
    g_state.viewAngles = params.angles;
    g_state.viewForward = params.forward;
    g_state.viewRight = params.right;
    g_state.viewUp = params.up;
    g_state.viewFrustum.Set(viewProjectionMatrix);

    g_state.viewMatrix = viewMatrix;
    g_state.projectionMatrix = projectionMatrix;
    g_state.viewProjectionMatrix = viewProjectionMatrix;

    g_state.sceneHasFog = g_state.inWater || g_state.fogEnabled;

    // update these mofos
    platformSetViewInfo(params.origin, params.forward, params.right, params.up);

    UpdateFrameConstants(vmViewProjectionMatrix);

    UpdateFogConstants();
}

static void SceneRenderPass(const SceneParams &params, bool onlyClientDraw)
{
    SetupViewport(params);

    entitySortTranslucents(params.origin);

    lightstyleUpdate();

    SetupView(params);

    if (!onlyClientDraw)
    {
        // animate the viewmodel, draw last
        entityDrawViewmodel(STUDIO_EVENTS);

        // draw world and solid brush entities
        entityDrawSolidBrushes();

        // solid studio models and sprites
        entityDrawSolidEntities();
    }

    // solid triapi draw
    {
        triapiBegin();
        HUD_DrawNormalTriangles();
        triapiEnd();
    }

    if (!onlyClientDraw)
    {
        entityDrawTranslucentEntities(params.origin, params.forward);
    }

    // entityDrawTranslucentEntities might have left this at a bogus value so reset it
    // i guess we don't want fog for the rest
    renderFogEnable(false);

    // transparent triapi draw
    {
        triapiBegin();
        HUD_DrawTransparentTriangles();
        triapiEnd();
    }

    if (!onlyClientDraw)
    {
        particleDraw();
        beamDraw();

        // draw the viewmodel last, can't draw it first
        // even though it covers a large part of the screen
        entityDrawViewmodel(STUDIO_RENDER);
    }

    // check errors before draw calls...
    GL_ERRORS();
}

void RenderScene(const Params &params)
{
    if (!g_state.active)
    {
        return;
    }

    g_state.inFrame = true;
    g_state.frameCount++;

    // clear engine errors
    GL_ERRORS_QUIET();

    // not the ideal place for this, but ok
    shaderUpdate();

    // not the ideal place for this, but ok
    textureUpdate();

    // not recorded to the command buffer
    gammaBindLUTs();

    SetupState();

    // constant state setup
    ref_params_t *refParams = params.refParams;
    {
        g_state.movevars = refParams->movevars;
        g_state.crosshairAngle = refParams->crosshairangle;
        g_state.inWater = refParams->waterlevel > 2;

        // update movevars for studio model lighting
        studioUpdateSkyLight(refParams->movevars);
    }

    // update the skybox texture if it has changed
    // kinda shit place to do this but we conveniently get the sky name from the ref parms...
    skyboxUpdate(g_state.movevars->skyName);

    commandRecord();
    dynamicBuffersMap();

    {
        SceneParams sceneParams;
        sceneParams.origin = refParams->vieworg;
        sceneParams.angles = refParams->viewangles;
        AngleVectors(sceneParams.angles, &sceneParams.forward, &sceneParams.right, &sceneParams.up);
        sceneParams.fov = params.fov;
        sceneParams.viewModelFov = params.viewModelFov;
        sceneParams.viewport_x = refParams->viewport[0];
        sceneParams.viewport_y = refParams->viewport[1];
        sceneParams.viewport_w = refParams->viewport[2];
        sceneParams.viewport_h = refParams->viewport[3];

        SceneRenderPass(sceneParams, refParams->onlyClientDraw);
    }

    dynamicBuffersUnmap();
    commandExecute();

    // this fucking sucks, actually
    if (!refParams->onlyClientDraw)
    {
        screenFadeDraw();
    }

    // back to fixed function
    RestoreState();

    g_state.inFrame = false;
}

}
