// interface exposed for the client-side plugin loader
#include "stdafx.h"

#if defined(_MSC_VER)
#define EXPORT extern "C" __declspec(dllexport)
#elif defined(__GNUC__)
#define EXPORT extern "C" __attribute__((visibility("default")))
#endif

struct ClientInterface
{
    int (*AddEntity)(int, cl_entity_s *, const char *);
    void (*DrawNormalTriangles)(void);
    void (*DrawTransparentTriangles)(void);
};

struct RenderInterface
{
    void (*ModifyEngfuncs)(cl_enginefuncs_s *);
    void (*Initialize)(engine_studio_api_s *, r_studio_interface_s **);
    int (*BeginFrame)(void);
    void (*RenderScene)(const Render::Params &);
    int (*AddEntity)(int, cl_entity_s *);
    Render::AddEntityCallback (*GetAddEntityCallback)(Render::AddEntityCallback);
    void (*PreDrawHud)(void);
    void (*PostDrawHud)(int, int);
};

static ClientInterface s_clientInterface;

static const RenderInterface s_renderInterface = {
    Render::ModifyEngfuncs,
    Render::Initialize,
    Render::BeginFrame,
    Render::RenderScene,
    Render::AddEntity,
    Render::GetAddEntityCallback,
    Render::PreDrawHud,
    Render::PostDrawHud
};

EXPORT int LoaderConnect(ClientInterface *client, RenderInterface *render, int renderSize, int paramsSize)
{
    if (renderSize != sizeof(RenderInterface))
    {
        return 0;
    }

    if (paramsSize != sizeof(Render::Params))
    {
        return 0;
    }

    s_clientInterface = *client;
    *render = s_renderInterface;

    return 1;
}

extern "C" int HUD_AddEntity(int a, cl_entity_s *b, const char *c)
{
    return s_clientInterface.AddEntity(a, b, c);
}

extern "C" void HUD_DrawNormalTriangles(void)
{
    s_clientInterface.DrawNormalTriangles();
}

extern "C" void HUD_DrawTransparentTriangles(void)
{
    s_clientInterface.DrawTransparentTriangles();
}
