#ifndef R_DECALS_H
#define R_DECALS_H

namespace Render
{

struct gl3_worldmodel_t;
struct gl3_surface_t;
class DynamicIndexState;

void decalInit();

// queue decals associated with this surface for rendering
void decalAddFromSurface(gl3_worldmodel_t *model, gl3_surface_t *surface);

// draw the queued decals, called for each brush model
// relies on state set by the brush renderer (shaders, etc.)
void decalDrawAll(DynamicIndexState &indexState);

}

#endif // R_DECALS_H
