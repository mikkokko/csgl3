// brush.h - brush moments
#ifndef BRUSH_H
#define BRUSH_H

namespace Render
{

// must match the engine
enum
{
    SURF_BACK = (1 << 1),
    SURF_SKY = (1 << 2),
    SURF_WATER = (1 << 4),
    SURF_SCROLL = (1 << 5),
    SURF_UNDERWATER = (1 << 7)
};

// must match the engine
enum
{
    TEX_SPECIAL = (1 << 0)
};

struct gl3_surface_t;

struct gl3_plane_t
{
    Vector3 normal;
    float dist;
    int type;

    // temp slop for testing cpu backface culling
    int cullframe;
    bool cullside;
};

struct gl3_texture_t
{
    char name[16];
    unsigned int width;
    unsigned int height;
    int gl_texturenum;
    gl3_surface_t *texturechain;
    int anim_total;
    int anim_min;
    int anim_max;
    gl3_texture_t *anim_next;
    gl3_texture_t *alternate_anims;
};

struct gl3_node_t
{
    int contents;
    int pvsframe;
    Vector3 mins;
    Vector3 maxs;
    gl3_node_t *parent;

    gl3_plane_t *plane;
    gl3_node_t *children[2];
};

struct gl3_leaf_t
{
    int contents;
    int pvsframe;
    Vector3 mins;
    Vector3 maxs;
    gl3_node_t *parent;

    byte *compressed_vis;
    gl3_surface_t **firstmarksurface;
    int nummarksurfaces;

    bool has_visible_surfaces;
};

struct gl3_surface_t
{
    int visframe;
    gl3_plane_t *plane;

    int flags;

    int firstvert;
    int numverts;

    gl3_surface_t *texturechain;
    gl3_texture_t *texture;
    byte styles[MAXLIGHTMAPS];

    // redundant bloat added for lightmap packing so
    // it doesn't depend on internal engine structures
    int lightmap_width;
    int lightmap_height;
    Color24 *lightmap_data;
    int style_count;

    // ugh... added for decals
    int lightmap_x, lightmap_y;

    int numindices;
    void *indices;
};

struct gl3_brushvert_t
{
    // lightmap width is stored in a_position.w
    Vector4 position;
    Vector4 texCoord;
    uint8_t styles[4];
};

struct gl3_worldmodel_t
{
    // engine equivalent of this model
    model_t *engine_model;

    int numplanes;
    gl3_plane_t *planes;

    // this is because of fucked up quake stuff
    int numleafs_total;
    int numleafs;
    gl3_leaf_t *leafs;

    int numnodes;
    gl3_node_t *nodes;

    int numsurfaces;
    gl3_surface_t *surfaces;

    int nummarksurfaces;
    gl3_surface_t **marksurfaces;

    int numtextures;
    gl3_texture_t *textures;

    GLuint vertex_buffer;

    // lightmap atlas size added for decals...
    GLuint lightmap_texture;
    int lightmap_width, lightmap_height;

    // 2 or 4, use u16 indices if possible to
    // halve the dynamic index buffer size
    int index_size;
};

// for sky and friends
extern const VertexFormat g_brushVertexFormat;

extern gl3_worldmodel_t g_worldmodel_static;
#define g_worldmodel (&(g_worldmodel_static))

void brushLoadWorldModel(model_t *engineModel);
void brushFreeWorldModel();

void brushInit();

void brushDrawSolids(
    cl_entity_t **entities,
    int entityCount,
    cl_entity_t **alphaEntities,
    int alphaEntityCount);

void brushDrawTranslucent(cl_entity_t *entity, float blend);
void brushEndTranslucents();

}

#endif
