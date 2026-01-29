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

    // flags added by me
    SURF_MULTI_STYLE = (1 << 30)
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
};

struct gl3_texture_t
{
    char name[16];
    unsigned int width;
    unsigned int height;
    int gl_texturenum;

    int numdrawsurfaces;
    gl3_surface_t **drawsurfaces;
    int surfflags; // msurface_t flags that only change per texture
    int basevertex;

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
    Vector3 center;
    Vector3 extents;
    gl3_node_t *parent;
    bool has_visible_surfaces;

    gl3_plane_t *plane;
    gl3_node_t *children[2];

    int firstsurface;
    int numsurfaces;
};

struct gl3_leaf_t
{
    int contents;
    int pvsframe;
    Vector3 center;
    Vector3 extents;
    gl3_node_t *parent;
    bool has_visible_surfaces;

    byte *compressed_vis;
    int *firstmarksurface;
    int nummarksurfaces;
};

struct gl3_surface_t
{
    int flags;
    gl3_texture_t *texture;

    int firstvert;
    int numverts;

    gl3_plane_t *plane;
};

// separate struct made for lightmap packing so
// it doesn't depend on internal engine structures
// also used by decal code when computing lightmap texcoords
struct gl3_fatsurface_t
{
    int firstvert;
    int numverts;

    int lightmap_width;
    int lightmap_height;
    Color24 *lightmap_data;
    int style_count;

    // ugh... added for decals
    int lightmap_x, lightmap_y;
    byte styles[MAXLIGHTMAPS];
};

inline uint16_t STORE_U16(float flt)
{
    return (uint16_t)flt;
}

inline uint16_t PACK_U16(float flt)
{
    return (uint16_t)(Lerp(0, UINT16_MAX, flt));
}

struct gl3_brushvert_t
{
    // lightmap width is stored in a_position.w
    Vector4 position;
    Vector2 texCoord;
    uint16_t lightmapTexCoord[2];
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
    gl3_fatsurface_t *fatsurfaces;

    int nummarksurfaces;
    gl3_surface_t **marksurfaces;

    int numtextures;
    gl3_texture_t *textures;

    GLuint vertex_buffer;

    // lightmap atlas size added for decals...
    GLuint lightmap_texture;
    int lightmap_width, lightmap_height;

    // max amount of indices world geometry and all inline models may have
    int max_index_count;

    // max amount of indices a single inline model may have
    int max_submodel_index_count;
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
