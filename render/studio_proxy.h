#ifndef STUDIO_PROXY_H
#define STUDIO_PROXY_H

namespace Render
{

struct StudioCache;
struct StudioSubModel;

// per-entity rendering state
struct StudioContext
{
    cl_entity_t *entity;
    float blend; // set per entity (FIXME: StudioSetRenderamt should update this?)
    studiohdr_t *header;
    model_t *model;
    StudioCache *cache;

    mstudiomodel_t *submodel;
    StudioSubModel *rendererSubModel;

    // lighting state
    float ambientlight;
    float shadelight;
    Vector3 lightcolor;
    Vector3 lightvec;

    int elightCount;
    Vector3 elightPositions[STUDIO_MAX_ELIGHTS];
    Vector4 elightColors[STUDIO_MAX_ELIGHTS]; // 4th component stores radius^2

    // FIXME: reconsider
    int rendermode;
};

void studioProxyInit(struct engine_studio_api_s *studio);

void studioProxyDrawEntity(int flags, cl_entity_t *entity, float blend);

}

#endif
