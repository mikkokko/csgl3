#ifndef STUDIOCACHE_H
#define STUDIOCACHE_H

//#define STUDIO_TANGENTS

namespace Render
{

struct StudioMesh
{
    unsigned indexOffset_notbytes;
    unsigned indexCount;
};

struct StudioSubModel
{
    StudioMesh *meshes;
};

struct StudioBodypart
{
    StudioSubModel *models;
};

struct StudioVertex
{
    Vector3 position;
    Vector3 normal;
    Vector2 texCoord;
#ifdef STUDIO_TANGENTS
    Vector4 tangent;
#endif
    float bone;
};

struct StudioCache
{
    char fileName[64];
    int fileLength;

    StudioBodypart *bodyparts;

    int indexSize;
    GLuint vertexBuffer;
    GLuint indexBuffer;
};

StudioCache *studioCacheGet(model_t *model, studiohdr_t *header);
StudioCache *studioCacheGet(cl_entity_t *entity);

void studioCacheTouchAll();

}

#endif
