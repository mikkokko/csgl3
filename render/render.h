#ifndef RENDER_H
#define RENDER_H

namespace Render
{

// global renderer state
struct State
{
    // set at startup
    GLuint whiteTexture;

    // set to true at the start of a frame if
    // we're rendering with the custom renderer
    bool active;

    // set to true when within the RenderScene block
    bool inFrame;
    int frameCount; // incremented in RenderScene

    // frame statistics
#ifdef SCHIZO_DEBUG
    int vertexBufferSize;
    int indexBufferSize;
    int uniformBufferSize;
    int drawcallCount;
    int commandBufferSize;
#endif

    // set when RenderScene is called
    movevars_t *movevars; //  used for studio model lighting params
    Vector3 crosshairAngle;

    // SetupView
    Vector3 viewOrigin;
    Vector3 viewAngles;
    Vector3 viewForward, viewRight, viewUp;
    ViewFrustum viewFrustum;

    // FIXME: reconsider
    Matrix4 viewMatrix;
    Matrix4 projectionMatrix;
    Matrix4 viewProjectionMatrix;

    int dlightCount;

    // actual fog switch, either triapi fog or water fog
    bool sceneHasFog;

    // fog parameters set by triapi
    bool fogEnabled;
    Vector3 fogColor;
    float fogDensity;
    bool fogSkybox;

    // for underwater fog
    bool inWater;
    Color32 waterColor;
};

extern cl_enginefunc_t g_engfuncs;
extern r_studio_interface_t **g_pstudio;
extern engine_studio_api_t g_engineStudio;

extern State g_state;

// ugh
extern dlight_t *g_dlights;
extern dlight_t *g_elights;

extern cvar_t *r_fullbright;
extern cvar_t *v_direct;
extern cvar_t *gl_spriteblend;

void renderFogEnable(bool enable, bool forceUpdate = false);

}

#endif
