#include "stdafx.h"
#include "brush.h"
#include "commandbuffer.h"
#include "decal.h"
#include "dynamicbuffer.h"
#include "lightmap.h"
#include "memory.h"
#include "pvs.h"
#include "skybox.h"
#include "water.h"
#include "internal.h"

namespace Render
{

// per brush model constant buffer
struct BrushConstants
{
    Matrix3x4 modelMatrix;
    Vector4 renderColor;
};

struct BrushShader : BaseShader
{
    GLint u_scroll;
};

static const VertexAttrib s_vertexAttribs[] = {
    { &gl3_brushvert_t::position, "a_position" },
    { &gl3_brushvert_t::texCoord, "a_texCoord" },
    { &gl3_brushvert_t::lightmapTexCoord, "a_lightmapTexCoord", true },
    { &gl3_brushvert_t::styles, "a_styles" }
};

const VertexFormat g_brushVertexFormat{ sizeof(gl3_brushvert_t), s_vertexAttribs };

static const ShaderUniform s_uniforms[] = {
    { "u_texture", 0 },
    { "u_lightmap", 1 },
    { "u_scroll", &BrushShader::u_scroll }
};

static constexpr ShaderOption s_shaderOptions[] = {
    { "ALPHA_TEST", 1 },
    { "MULTI_STYLE", 1 },
    { "HAS_DLIGHTS", 1 }
};

// must match s_shaderOptions
struct BrushShaderOptions
{
    unsigned alphaTest;
    unsigned multiStyle;
    unsigned hasDlights;
};

// lightmapped shaders
static BrushShader s_shaders[shaderVariantCount(s_shaderOptions)];

// not lightmapped, color modulated by renderColor
static BrushShader s_shaderUnlit;

gl3_worldmodel_t g_worldmodel_static;

// should be correct? accessed with signed 16 bit ints
#define MAX_SURFACES 32768
static uint32_t s_surfaceVisBits[MAX_SURFACES / 32];

static int s_indexCount;
static int s_indexLastDraw;
static BufferSpanT<uint16_t> s_indexSpan;

// this is so dumb
static bool s_hasWaterSurfaces = false;
static bool s_hasSkySurfaces = false;

void brushInit()
{
    shaderRegister(s_shaders, "lightmapped", s_vertexAttribs, s_uniforms, s_shaderOptions);
    shaderRegister(s_shaderUnlit, "unlit", s_vertexAttribs, s_uniforms);
}

static void BuildLightmapAndVertexBuffer(model_t *model)
{
    TempMemoryScope temp;

    int num_verts;
    gl3_brushvert_t *vertex_buffer = internalBuildVertexBuffer(model, g_worldmodel, num_verts, temp);

    // lightmap building modifies the lightmap texcoords, so do it here before the vertex data gets uploaded
    g_worldmodel->lightmap_texture = lightmapCreateAtlas(g_worldmodel, vertex_buffer);

    glGenBuffers(1, &g_worldmodel->vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, g_worldmodel->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(*vertex_buffer) * num_verts, vertex_buffer, GL_STATIC_DRAW);
}

void brushLoadWorldModel(model_t *engineModel)
{
    memset(g_worldmodel, 0, sizeof(*g_worldmodel));
    internalLoadBrushModel(engineModel, g_worldmodel);
    BuildLightmapAndVertexBuffer(engineModel);
}

void brushFreeWorldModel()
{
    // if these are zero, opengl will do nothing
    glDeleteBuffers(1, &g_worldmodel->vertex_buffer);
    glDeleteTextures(1, &g_worldmodel->lightmap_texture);
}

static void MapIndexBuffer(int maxIndices)
{
    GL3_ASSERT(!s_indexCount);
    GL3_ASSERT(!s_indexLastDraw);

    s_indexSpan = dynamicIndexDataBegin<uint16_t>(maxIndices);

    commandBindIndexBuffer(s_indexSpan.buffer);
}

static void UnmapIndexBuffer()
{
    dynamicIndexDataEnd<uint16_t>(s_indexCount);
    s_indexCount = 0;
    s_indexLastDraw = 0;
}

static void DrawIndexBuffer(int baseVertex)
{
    int indexCount = s_indexCount - s_indexLastDraw;
    if (!indexCount)
    {
        return;
    }

    GL3_ASSERT(indexCount > 0);

    int byteOffset = s_indexSpan.byteOffset + (s_indexLastDraw * sizeof(uint16_t));

    commandDrawElementsBaseVertex(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, byteOffset, baseVertex);

    s_indexLastDraw = s_indexCount;
}

static void AddSurfaceToIndexBuffer(gl3_surface_t *surface)
{
    // can overflow due to decal indices, but unlikely...
    int numtris = surface->numverts - 2;

    uint16_t *dest = &s_indexSpan.data[s_indexCount];
    s_indexCount += (numtris * 3);

    uint16_t firstvert = static_cast<uint16_t>(surface->firstvert);
    uint16_t current = firstvert + 1;

    for (int i = 0; i < numtris; i++)
    {
        dest[0] = firstvert;
        dest[1] = current;
        dest[2] = current + 1;
        current++;
        dest += 3;
    }
}

static gl3_texture_t *TextureAnimation(cl_entity_t *entity, gl3_texture_t *texture)
{
    if (entity && entity->curstate.frame && texture->alternate_anims)
    {
        texture = texture->alternate_anims;
    }

    if (!texture->anim_total)
    {
        return texture;
    }

    if (texture->name[0] != '-')
    {
        int index = (int)(g_engfuncs.GetClientTime() * 10) % texture->anim_total;

        for (int i = 0; texture->anim_min > index || texture->anim_max <= index; i++)
        {
            GL3_ASSERT(i <= 100);
            texture = texture->anim_next;
            GL3_ASSERT(texture);
        }
    }

    return texture;
}

// set when creating texture sorted draw lists (FIXME)
static bool s_multiStyle;

static void AddSurface_NoDecals(gl3_surface_t *surface)
{
    int flags = surface->flags;
    s_multiStyle |= ((flags & SURF_MULTI_STYLE) != 0);

    gl3_texture_t *texture = surface->texture;
    texture->drawsurfaces[texture->numdrawsurfaces++] = surface;
}

void TraverseTree_r(gl3_node_t *node)
{
    if (node->pvsframe != g_pvsFrame)
    {
        return;
    }

    // shouldn't be visiting nodes with no visible surfaces, and solid leaves are not drawn
    GL3_ASSERT(node->has_visible_surfaces);
    GL3_ASSERT(node->contents != CONTENTS_SOLID);

    if (g_state.viewFrustum.CullBox(node->center, node->extents))
    {
        return;
    }

    if (node->contents < 0)
    {
        gl3_leaf_t *leaf = (gl3_leaf_t *)node;
        GL3_ASSERT(leaf->has_visible_surfaces);

        int *begin = leaf->firstmarksurface;
        int *end = begin + leaf->nummarksurfaces;

        for (int *mark = begin; mark < end; mark++)
        {
            int i = *mark;
            s_surfaceVisBits[i >> 5] |= (1 << (i & 31));
        }

        return;
    }

    gl3_plane_t *plane = node->plane;
    int side = Dot(plane->normal, g_state.viewOrigin) < plane->dist;
    int sideFlag = side ? SURF_BACK : 0;

    TraverseTree_r(node->children[side]);

    int begin = node->firstsurface;
    int end = begin + node->numsurfaces;

    for (int i = begin; i < end; i++)
    {
        if (!(s_surfaceVisBits[i >> 5] & (1 << (i & 31))))
        {
            continue;
        }

        gl3_surface_t *surface = &g_worldmodel->surfaces[i];
        if ((surface->flags & SURF_BACK) != sideFlag)
        {
            continue;
        }

        AddSurface_NoDecals(surface);
        internalSurfaceDecals(g_worldmodel, i);
    }

    TraverseTree_r(node->children[!side]);
}

static void LinkLeaves()
{
    pvsUpdate(g_state.viewOrigin);

    // would rather do this than store g_state.frameCount in gl3_surface_t
    GL3_ASSERT(g_worldmodel->numsurfaces < MAX_SURFACES);
    memset(s_surfaceVisBits, 0, (g_worldmodel->numsurfaces + 7) / 8);

    //int clipFlags = (1 << 4) - 1;
    GL3_ASSERT(!s_multiStyle);
    TraverseTree_r(g_worldmodel->nodes);
}

static float ScrollAmount(cl_entity_t *entity, gl3_texture_t *texture)
{
    if (!entity)
    {
        return 0;
    }

    if (!(texture->surfflags & SURF_SCROLL))
    {
        return 0;
    }

    float speed = (float)(entity->curstate.rendercolor.b + (entity->curstate.rendercolor.g << 8)) / 16.0f;
    if (!entity->curstate.rendercolor.r)
    {
        speed = -speed;
    }

    float scroll = 1.0f / (float)texture->width * g_engfuncs.GetClientTime() * speed;
    if (scroll < 0)
    {
        return fmodf(scroll, -1);
    }
    else
    {
        return fmodf(scroll, 1);
    }
}

static void DrawSurfaces(cl_entity_t *entity, GLint scrollUniformLocation, GLuint textureOverride)
{
    // index buffer is dynamic
    commandBindVertexBuffer(g_worldmodel->vertex_buffer, g_brushVertexFormat);

    float prevScroll = 0;
    commandUniform1f(scrollUniformLocation, prevScroll);

    if (textureOverride)
    {
        commandBindTexture(0, GL_TEXTURE_2D, textureOverride);
    }

    for (int i = 0; i < g_worldmodel->numtextures; i++)
    {
        gl3_texture_t *texture = &g_worldmodel->textures[i];
        if (!texture->numdrawsurfaces)
        {
            continue;
        }

        if (texture->surfflags & SURF_WATER)
        {
            // drawn later
            s_hasWaterSurfaces = true;
            continue;
        }

        if (texture->surfflags & SURF_SKY)
        {
            // drawn later
            s_hasSkySurfaces = true;
            continue;
        }

        float scroll = ScrollAmount(entity, texture);
        if (scroll != prevScroll)
        {
            DrawIndexBuffer(texture->basevertex);
            prevScroll = scroll;
            commandUniform1f(scrollUniformLocation, prevScroll);
        }

        if (!textureOverride)
        {
            GLuint textureName = TextureAnimation(entity, texture)->gl_texturenum;
            commandBindTexture(0, GL_TEXTURE_2D, textureName);
        }

        for (int j = 0; j < texture->numdrawsurfaces; j++)
        {
            gl3_surface_t *surface = texture->drawsurfaces[j];
            AddSurfaceToIndexBuffer(surface);
        }

        DrawIndexBuffer(texture->basevertex);

        texture->numdrawsurfaces = 0;
    }
}

static void DrawWaterSurfaces(cl_entity_t *entity, GLuint textureOverride)
{
    // restore the vertex buffer since drawing decals might have changed it
    commandBindVertexBuffer(g_worldmodel->vertex_buffer, g_brushVertexFormat);

    if (textureOverride)
    {
        commandBindTexture(0, GL_TEXTURE_2D, textureOverride);
    }

    waterDrawBegin();

    // like in opengl, water color is determined by the last surface drawn
    int lastDrawnTexture = -1;

    for (int i = 0; i < g_worldmodel->numtextures; i++)
    {
        gl3_texture_t *texture = &g_worldmodel->textures[i];
        if (!texture->numdrawsurfaces)
        {
            continue;
        }

        if (!(texture->surfflags & SURF_WATER))
        {
            continue;
        }

        lastDrawnTexture = i;

        if (!textureOverride)
        {
            GLuint textureName = TextureAnimation(entity, texture)->gl_texturenum;
            commandBindTexture(0, GL_TEXTURE_2D, textureName);
        }

        for (int j = 0; j < texture->numdrawsurfaces; j++)
        {
            gl3_surface_t *surface = texture->drawsurfaces[j];
            AddSurfaceToIndexBuffer(surface);
        }

        DrawIndexBuffer(texture->basevertex);

        texture->numdrawsurfaces = 0;
    }

    
    if (lastDrawnTexture != -1)
    {
        g_state.waterColor = internalWaterColor(g_worldmodel->engine_model, lastDrawnTexture);
    }

    waterDrawEnd();
}

static void DrawSkySurfaces()
{
    if (!skyboxDrawBegin())
    {
        // nothing to draw
        return;
    }

    // restore the vertex buffer since drawing decals might have changed it
    commandBindVertexBuffer(g_worldmodel->vertex_buffer, g_brushVertexFormat);

    for (int i = 0; i < g_worldmodel->numtextures; i++)
    {
        gl3_texture_t *texture = &g_worldmodel->textures[i];
        if (!texture->numdrawsurfaces)
        {
            continue;
        }

        if (!(texture->surfflags & SURF_SKY))
        {
            continue;
        }

        for (int j = 0; j < texture->numdrawsurfaces; j++)
        {
            gl3_surface_t *surface = texture->drawsurfaces[j];
            AddSurfaceToIndexBuffer(surface);
        }

        DrawIndexBuffer(texture->basevertex);

        texture->numdrawsurfaces = 0;
    }

    skyboxDrawEnd();
}

// draws all surfaces, decals, water and sky associated with this model
static void DrawAllSurfaces(cl_entity_t *entity, bool lightmapped, bool alphaTest, GLuint textureOverride)
{
    BrushShader *shader;

    bool multiStyle = s_multiStyle;
    s_multiStyle = 0;

    if (lightmapped)
    {
        //g_engfuncs.Con_Printf("multi styles? %d\n", multiStyle ? 1 : 0);

        BrushShaderOptions options{};
        options.alphaTest = alphaTest;
        options.multiStyle = multiStyle;
        options.hasDlights = (g_state.dlightCount > 0);
        shader = &shaderSelect(s_shaders, s_shaderOptions, options);
    }
    else
    {
        shader = &s_shaderUnlit;
    }

    commandUseProgram(shader);

    DrawSurfaces(entity, shader->u_scroll, textureOverride);

    // decal indices are stuffed into the same index buffer
    GL3_ASSERT(s_indexCount == s_indexLastDraw);
    s_indexCount = decalDrawAll(s_indexSpan.data, s_indexSpan.byteOffset, s_indexCount);
    s_indexLastDraw = s_indexCount;

    if (s_hasWaterSurfaces)
    {
        DrawWaterSurfaces(entity, textureOverride);
        s_hasWaterSurfaces = false;
    }

    if (s_hasSkySurfaces)
    {
        DrawSkySurfaces();
        s_hasSkySurfaces = false;
    }
}

static void BrushModelCenterExtents(cl_entity_t *entity, Vector3 &center, Vector3 &extents)
{
    model_t *model = entity->model;

    if (!VectorIsZero(entity->angles))
    {
        center = entity->origin;
        extents = { model->radius, model->radius, model->radius };
    }
    else
    {
        center = entity->origin + (model->mins + model->maxs) * 0.5f;
        extents = (model->maxs - model->mins) * 0.5f;
    }
}

// FIXME: dumbm, trying to match engine water surface culling
static float BrushModelMinz(cl_entity_t *entity)
{
    model_t *model = entity->model;

    if (!VectorIsZero(entity->angles))
    {
        return entity->origin.z - model->radius;
    }
    else
    {
        return entity->origin.z + model->mins.z;
    }
}

static bool CullBrushModel(cl_entity_t *entity)
{
    Vector3 center, extents;
    BrushModelCenterExtents(entity, center, extents);
    return g_state.viewFrustum.CullBox(center, extents);
}

static void LinkAndDrawBrushModel(cl_entity_t *entity, bool lightmapped, bool alphaTest)
{
    // FIXME: this is assuming all brush models are inline models
    model_t *model = entity->model;

    // no decals on alpha tested surfaces
    bool addDecals = (entity->curstate.rendermode != kRenderTransAlpha);

    int begin = model->firstmodelsurface;
    int end = begin + model->nummodelsurfaces;

    float minz = BrushModelMinz(entity);

    GL3_ASSERT(!s_multiStyle);

    for (int surfaceIndex = begin; surfaceIndex < end; surfaceIndex++)
    {
        gl3_surface_t *surface = &g_worldmodel->surfaces[surfaceIndex];

        // cull non-top water faces
        if (surface->flags & SURF_WATER)
        {
            gl3_plane_t *plane = surface->plane;
            if (plane->type != PLANE_Z || minz + 1 >= plane->dist)
            {
                // get culled fuckass
                continue;
            }
        }

        AddSurface_NoDecals(surface);

        if (addDecals)
        {
            internalSurfaceDecals(g_worldmodel, surfaceIndex);
        }
    }

    // stupid hack for color rendermode
    GLuint textureOverride = 0;

    if (entity->curstate.rendermode == kRenderTransColor)
    {
        textureOverride = g_state.whiteTexture;
    }

    DrawAllSurfaces(entity, lightmapped, alphaTest, textureOverride);
}

static void SetupConstantBuffer(cl_entity_t *entity, const Vector4 &renderColor)
{
    BrushConstants constants;

    if (entity)
    {
        constants.modelMatrix = ModelMatrix3x4(entity->origin, entity->angles);
    }
    else
    {
        constants.modelMatrix = DiagonalMatrix3x4(1.0f);
    }

    constants.renderColor = renderColor;

    BufferSpan span = dynamicUniformData(&constants, sizeof(constants));
    commandBindUniformBuffer(1, span.buffer, span.byteOffset, sizeof(constants));
}

static void LinkAndDrawWorldModel()
{
    SetupConstantBuffer(nullptr, { 1, 1, 1, 1 });

    // setup texturechains, water chain and decals
    LinkLeaves();

    // no associated entity, lightmapped, not alpha tested, no texture override
    DrawAllSurfaces(nullptr, true, false, 0);
}

void brushDrawSolids(
    cl_entity_t **entities,
    int entityCount,
    cl_entity_t **alphaEntities,
    int alphaEntityCount)
{
    // lightmap only used for solid brush entities
    commandBindTexture(1, GL_TEXTURE_2D, g_worldmodel->lightmap_texture);

    MapIndexBuffer(g_worldmodel->max_index_count);

    // draw fully opaque stuff
    {
        LinkAndDrawWorldModel();

        for (int i = 0; i < entityCount; i++)
        {
            // FIXME: could cull earlier...
            if (CullBrushModel(entities[i]))
            {
                continue;
            }

            SetupConstantBuffer(entities[i], { 1, 1, 1, 1 });
            LinkAndDrawBrushModel(entities[i], true, false);
        }
    }

    // draw alpha tested stuff
    {
        for (int i = 0; i < alphaEntityCount; i++)
        {
            // FIXME: could cull earlier...
            if (CullBrushModel(alphaEntities[i]))
            {
                continue;
            }

            SetupConstantBuffer(alphaEntities[i], { 1, 1, 1, 1 });
            LinkAndDrawBrushModel(alphaEntities[i], true, true);
        }
    }

    UnmapIndexBuffer();
}

// for translucent brush entities: determines render color, sets blending
static void SetBlendingAndGetColor(cl_entity_t *entity, Vector4 &renderColor, float blend)
{
    GL3_ASSERT(entity);

    // not lightmapped
    float r = entity->curstate.rendercolor.r * (1.0f / 255);
    float g = entity->curstate.rendercolor.g * (1.0f / 255);
    float b = entity->curstate.rendercolor.b * (1.0f / 255);
    float a = blend;

    switch (entity->curstate.rendermode)
    {
    case kRenderTransTexture:
        renderColor = { 1, 1, 1, a };
        commandBlendEnable(GL_TRUE);
        commandBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        commandDepthMask(GL_FALSE);
        break;

    case kRenderTransAdd:
        renderColor = { a, a, a, 1 };
        commandBlendEnable(GL_TRUE);
        commandBlendFunc(GL_ONE, GL_ONE);
        commandDepthMask(GL_FALSE);
        break;

    case kRenderTransColor:
        renderColor = { r, g, b, a };
        commandBlendEnable(GL_TRUE);
        commandBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        commandDepthMask(GL_FALSE);
        break;

    default:
        // wtf? this will render incorrectly
        GL3_ASSERT(false);
        renderColor = { 1, 1, 1, 1 };
        commandBlendEnable(GL_FALSE);
        commandDepthMask(GL_TRUE);
        break;
    }
}

void brushDrawTranslucent(cl_entity_t *entity, float blend)
{
    // FIXME: could cull earlier...
    if (CullBrushModel(entity))
    {
        return;
    }

    Vector4 renderColor;
    SetBlendingAndGetColor(entity, renderColor, blend);
    SetupConstantBuffer(entity, renderColor);

    MapIndexBuffer(g_worldmodel->max_submodel_index_count);

    // not lightmapped or alpha tested
    LinkAndDrawBrushModel(entity, false, false);

    UnmapIndexBuffer();
}

void brushEndTranslucents()
{
    // might leave blending enabled and depth mask disabled so check it here
    commandBlendEnable(GL_FALSE);
    commandDepthMask(GL_TRUE);
}

}
