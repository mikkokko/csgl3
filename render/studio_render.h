#ifndef STUDIO_RENDER_H
#define STUDIO_RENDER_H

namespace Render
{

struct StudioCache;
struct StudioContext;

extern const VertexFormat g_studioVertexFormat;

void studioRenderInit();

void studioBeginModels(bool viewmodel);
void studioEndModels();

void studioSetupModel(StudioContext &context, int bodypart, mstudiobodyparts_t **ppbodypart, mstudiomodel_t **ppsubmodel);
void studioEntityLight(StudioContext &context);
void studioSetupLighting(StudioContext &context, const alight_t *lighting);
void studioSetupRenderer(StudioContext &context, int rendermode);
void studioRestoreRenderer(StudioContext &context);
void studioDrawPoints(StudioContext &context);

}

#endif
