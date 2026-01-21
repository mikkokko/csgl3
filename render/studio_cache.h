#ifndef STUDIOCACHE_H
#define STUDIOCACHE_H

namespace Render
{

struct StudioMesh
{
    unsigned indexOffset_notbytes;
    unsigned indexCount;
    unsigned baseVertex;
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
    Vector2 texCoord;

    // store the bone as a float so we don't have to use glVertexAttribIPointer
    float bone;

    // pack normals to 24 bits... GL_INT_2_10_10_10_REV not available
    // and this is generally enough resolution (valve studiomdl quantizes
    // to 2 degrees of accuracy, int8 component should have around 0.6)
    int8_t normal[4];

    // could use for tangent or smooth normals
    int8_t padding[4];
};

struct StudioCache
{
    char fileName[64];
    int fileLength;

    StudioBodypart *bodyparts;

    GLuint vertexBuffer;
    GLuint indexBuffer;
};

StudioCache *studioCacheGet(model_t *model, studiohdr_t *header);
StudioCache *studioCacheGet(cl_entity_t *entity);

void studioCacheTouchAll();

}

#endif
