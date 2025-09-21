// gl_model.h - brush and sprite model structures
#ifndef MODEL_GOLDSRC_H
#define MODEL_GOLDSRC_H

// these never change, ever since 1998, always the same
// update: in the 25th anniversary update some idiot contractor changed msurface_t

namespace Render
{

namespace goldsrc
{

    enum modtype_t
    {
        mod_brush = 0,
        mod_sprite = 1,
        mod_alias = 2,
        mod_studio = 3
    };

    enum synctype_t
    {
        ST_SYNC = 0,
        ST_RAND = 1
    };

    struct dmodel_t
    {
        float mins[3];
        float maxs[3];
        float origin[3];
        int headnode[4];
        int visleafs;
        int firstface;
        int numfaces;
    };

    struct mplane_t
    {
        Vector3 normal;
        float dist;
        byte type;
        byte signbits;
        unsigned char pad[2];
    };

    struct mvertex_t
    {
        Vector3 position;
    };

    struct medge_t
    {
        unsigned short v[2];
        unsigned int cachededgeoffset;
    };

    struct msurface_t;

    struct texture_t
    {
        char name[16];
        unsigned int width;
        unsigned int height;
        int gl_texturenum;
        msurface_t *texturechain;
        int anim_total;
        int anim_min;
        int anim_max;
        texture_t *anim_next;
        texture_t *alternate_anims;
        unsigned int offsets[4];
        byte *pPal;
    };

    struct mtexinfo_t
    {
        Vector3 vec_s;
        float dist_s;
        Vector3 vec_t;
        float dist_t;
        float mipadjust;
        texture_t *texture;
        int flags;
    };

    struct mnode_t
    {
        int contents;
        int visframe;
        Vector3 mins;
        Vector3 maxs;
        mnode_t *parent;
        mplane_t *plane;
        mnode_t *children[2];
        unsigned short firstsurface;
        unsigned short numsurfaces;
    };

    struct decal_t
    {
        decal_t *pnext;
        msurface_t *psurface;
        float dx;
        float dy;
        float scale;
        short texture;
        short flags;
        short entityIndex;
    };

    struct mleaf_t
    {
        int contents;
        int visframe;
        Vector3 mins;
        Vector3 maxs;
        mnode_t *parent;
        byte *compressed_vis;
        efrag_t *efrags;
        msurface_t **firstmarksurface;
        int nummarksurfaces;
        int key;
        unsigned char ambient_sound_level[4];
    };

    struct glvert_t
    {
        Vector3 position;
        Vector2 texcoord;
        Vector2 lmtexcoord;
    };

    struct glpoly_t
    {
        glpoly_t *next;
        glpoly_t *chain;
        int numverts;
        int flags;
        glvert_t verts[4];
    };

    struct msurface_t
    {
        int visframe;
        mplane_t *plane;
        int flags;
        int firstedge;
        int numedges;
        short texturemins[2];
        short extents[2];
        int light_s;
        int light_t;
        glpoly_t *polys;
        msurface_t *texturechain;
        mtexinfo_t *texinfo;
        int dlightframe;
        int dlightbits;
        int lightmaptexturenum;
        byte styles[4];
        int cached_light[4];
        qboolean cached_dlight;
        color24 *samples;
        decal_t *pdecals;
    };

    struct mdisplaylist_t
    {
        unsigned int gl_displaylist;
        int rendermode;
        float scrolloffset;
        int renderDetailTexture;
    };

    struct msurface_new_t
    {
        int visframe;
        mplane_t *plane;
        int flags;
        int firstedge;
        int numedges;
        short texturemins[2];
        short extents[2];
        int light_s;
        int light_t;
        glpoly_t *polys;
        msurface_t *texturechain;
        mtexinfo_t *texinfo;
        int dlightframe;
        int dlightbits;
        int lightmaptexturenum;
        byte styles[4];
        int cached_light[4];
        qboolean cached_dlight;
        color24 *samples;
        decal_t *pdecals;
        mdisplaylist_t displaylist;
    };

    struct dclipnode_t
    {
        int planenum;
        short children[2];
    };

    struct hull_t
    {
        dclipnode_t *clipnodes;
        mplane_t *planes;
        int firstclipnode;
        int lastclipnode;
        Vector3 clip_mins;
        Vector3 clip_maxs;
    };

    struct cache_user_t
    {
        void *data;
    };

    struct model_t
    {
        char name[64];
        qboolean needload;
        modtype_t type;
        int numframes;
        synctype_t synctype;
        int flags;
        Vector3 mins;
        Vector3 maxs;
        float radius;
        int firstmodelsurface;
        int nummodelsurfaces;
        int numsubmodels;
        dmodel_t *submodels;
        int numplanes;
        mplane_t *planes;
        int numleafs;
        mleaf_t *leafs;
        int numvertexes;
        mvertex_t *vertexes;
        int numedges;
        medge_t *edges;
        int numnodes;
        mnode_t *nodes;
        int numtexinfo;
        mtexinfo_t *texinfo;
        int numsurfaces;
        msurface_t *surfaces;
        int numsurfedges;
        int *surfedges;
        int numclipnodes;
        dclipnode_t *clipnodes;
        int nummarksurfaces;
        msurface_t **marksurfaces;
        hull_t hulls[4];
        int numtextures;
        texture_t **textures;
        byte *visdata;
        color24 *lightdata;
        char *entities;
        cache_user_t cache;
    };

    struct mspriteframe_t
    {
        int width;
        int height;
        float up;
        float down;
        float left;
        float right;
        int gl_texturenum;
    };

    struct mspritegroup_t
    {
        int numframes;
        float *intervals;
        mspriteframe_t *frames[1];
    };

    enum spriteframetype_t
    {
        SPR_SINGLE,
        SPR_GROUP
    };

    struct mspriteframedesc_t
    {
        spriteframetype_t type;
        mspriteframe_t *frameptr;
    };

    struct msprite_t
    {
        short type;
        short texFormat;
        int maxwidth;
        int maxheight;
        int numframes;
        int paloffset;
        float beamlength;
        void *cachespot;
        mspriteframedesc_t frames[1];
    };

}

}

#endif //MODEL_GOLDSRC_H
