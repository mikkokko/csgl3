#ifndef RENDER_INTERFACE_H
#define RENDER_INTERFACE_H

#include "linmath.h"

struct ref_params_s;
struct cl_entity_s;
struct cl_enginefuncs_s;
struct engine_studio_api_s;
struct r_studio_interface_s;

namespace Render
{

struct Params
{
    float fov;
    float viewModelFov;
    ref_params_s *refParams;
};

using AddEntityCallback = int (*)(cl_entity_s *entity);

// call from the very start of Initialize
void ModifyEngfuncs(cl_enginefuncs_s *engfuncs);

// call from HUD_GetStudioModelInterface right after the STUDIO_INTERFACE_VERSION check
void Initialize(engine_studio_api_s *studio, r_studio_interface_s **pinterface);

// call from the end of HUD_Frame
// returns 1 if the renderer will be used to draw this frame
int BeginFrame();

// call from the end of V_CalcRefdef
void RenderScene(const Params &params);

// call from the end of HUD_AddEntity and return 1 if this returns 0
int AddEntity(int type, cl_entity_s *entity);

// replace the AddVisibleEntity callback in HUD_TempEntUpdate
// with the return value of this... clunky, but it is what it is
AddEntityCallback GetAddEntityCallback(AddEntityCallback original);

// call from HUD_Redraw beofre and after gHUD.Redraw
void PreDrawHud();
void PostDrawHud(int screenWidth, int screenHeight);

}

#endif // RENDER_INTERFACE_H
