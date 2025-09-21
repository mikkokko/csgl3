// these are the only functions that touch internal engine structures
#ifndef INTERNAL_H
#define INTERNAL_H

namespace Render
{

class TempMemoryScope;

struct gl3_worldmodel_t;
struct gl3_brushvert_t;

struct SpriteInfo
{
    int type;
    float up;
    float down;
    float left;
    float right;
    GLuint texture;
};

// pulling data from the engine worldmodel
bool internalLoadBrushModel(model_t *model, gl3_worldmodel_t *outModel);
gl3_brushvert_t *internalBuildVertexBuffer(model_t *model, gl3_worldmodel_t *outModel, int &vertexCount, TempMemoryScope &temp);
void *internalSurfaceDecals(model_t *model, void *lastDecal, int surfaceIndex, GLuint *texture, gl3_brushvert_t *vertices, int *vertexCount);

// calls GL_Bind(0) by any means necessary
void internalClearBoundTexture();

bool internalGetSpriteInfo(model_t *model, int frameIndex, SpriteInfo *result);

// for studio model lighting...
bool internalTraceLineToSky(model_t *model, const Vector3 &start, const Vector3 &end);
colorVec internalSampleLightmap(model_t *model, const Vector3 &start, const Vector3 &end);

// sets cl.weaponstarttime and cl.weaponsequence
void internalUpdateViewmodelAnimation(cl_entity_t *viewmodel);

// water color from texture
Color32 internalWaterColor(model_t *model, int textureIndex);

}

#endif
