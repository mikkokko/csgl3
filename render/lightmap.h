#ifndef R_LIGHTMAP_H
#define R_LIGHTMAP_H

namespace Render
{

struct gl3_worldmodel_t;
struct gl3_brushvert_t;

// creates the lightmap texture and updates the lightmap texcoords of vertices
// returns the GL texture name
GLuint lightmapCreateAtlas(gl3_worldmodel_t *model, gl3_brushvert_t *vertices);

}

#endif
