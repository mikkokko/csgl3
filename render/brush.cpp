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

// legacy trauma, used for setting the uniforms
struct BrushShaderOptions
{
    bool lightmapped;
};

static const VertexAttrib s_vertexAttribs[] = {
    VERTEX_ATTRIB(gl3_brushvert_t, position),
    VERTEX_ATTRIB(gl3_brushvert_t, texCoord),
    VERTEX_ATTRIB(gl3_brushvert_t, styles),
    VERTEX_ATTRIB_TERM()
};

const VertexFormat g_brushVertexFormat{ s_vertexAttribs, sizeof(gl3_brushvert_t) };

class BrushShader : public BaseShader
{
public:
    const char *Name()
    {
        return "brush";
    }

    const VertexAttrib *VertexAttribs()
    {
        return s_vertexAttribs;
    }

    const ShaderUniform *Uniforms()
    {
        static const ShaderUniform uniforms[] = {
            SHADER_UNIFORM_CONST(u_texture, 0),
            SHADER_UNIFORM_CONST(u_lightmap, 1),
            SHADER_UNIFORM_MUT(BrushShader, u_scroll),
            SHADER_UNIFORM_TERM()
        };

        return uniforms;
    }

    GLint u_scroll;
};

class BrushShaderAlphaTest : public BrushShader
{
public:
    const char *Defines()
    {
        return "#define ALPHA_TEST 1\n";
    }
};

// no lightmaps or dlights, color modulated by BrushConstants::renderColor
class BrushShaderNoLighting : public BrushShader
{
public:
    const char *Defines()
    {
        return "#define NO_LIGHTING 1\n";
    }
};

gl3_worldmodel_t g_worldmodel_static;

// water and sky are drawn with different shaders
static gl3_surface_t *s_waterSurfaces;
static gl3_surface_t *s_skySurfaces;

static BrushShader s_shader;
static BrushShaderAlphaTest s_shaderAlphaTest;
static BrushShaderNoLighting s_shaderNoLighting;

static cvar_t *gl3_brush_face_cull;

void brushInit()
{
    gl3_brush_face_cull = g_engfuncs.pfnRegisterVariable("gl3_brush_face_cull", "1", 0);

    shaderRegister(s_shader);
    shaderRegister(s_shaderAlphaTest);
    shaderRegister(s_shaderNoLighting);
}

static void BuildLightmapAndVertexBuffer(model_t *model, gl3_worldmodel_t *dstmod)
{
    TempMemoryScope temp;

    int num_verts;
    gl3_brushvert_t *vertex_buffer = internalBuildVertexBuffer(model, dstmod, num_verts, temp);

    // lightmap building modifies the lightmap texcoords, so do it here before the vertex data gets uploaded
    dstmod->lightmap_texture = lightmapCreateAtlas(g_worldmodel, vertex_buffer);

    glGenBuffers(1, &dstmod->vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, dstmod->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(*vertex_buffer) * num_verts, vertex_buffer, GL_STATIC_DRAW);
}

void brushLoadWorldModel(model_t *engineModel)
{
    memset(g_worldmodel, 0, sizeof(*g_worldmodel));
    internalLoadBrushModel(engineModel, g_worldmodel);
    BuildLightmapAndVertexBuffer(engineModel, g_worldmodel);
}

void brushFreeWorldModel()
{
    // if these are zero, opengl will do nothing
    glDeleteBuffers(1, &g_worldmodel->vertex_buffer);
    glDeleteTextures(1, &g_worldmodel->lightmap_texture);
}

static void MapIndexBuffer()
{
    g_dynamicIndexState.Lock(g_worldmodel->index_size);
    commandBindIndexBuffer(g_dynamicIndexState.IndexBuffer());
}

static void UnmapIndexBuffer()
{
    g_dynamicIndexState.Unlock();
}

static void DrawIndexBuffer()
{
    int indexByteOffset;
    int indexCount = g_dynamicIndexState.UpdateDrawOffset(indexByteOffset);
    if (!indexCount)
    {
        return;
    }

    commandDrawElements(GL_TRIANGLES, indexCount, g_dynamicIndexState.IndexType(), indexByteOffset);
}

static void AddSurfaceToIndexBuffer(gl3_surface_t *surface)
{
    g_dynamicIndexState.Write(surface->indices, surface->numindices);
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

static bool SideCameraIsOn(const gl3_plane_t *plane)
{
    switch (plane->type)
    {
    case PLANE_X:
        return g_state.viewOrigin.x < plane->dist;
    case PLANE_Y:
        return g_state.viewOrigin.y < plane->dist;
    case PLANE_Z:
        return g_state.viewOrigin.z < plane->dist;
    }

    return Dot(plane->normal, g_state.viewOrigin) < plane->dist;
}

static bool CacheSideCameraIsOn(gl3_plane_t *plane)
{
    if (plane->cullframe != g_state.frameCount)
    {
        plane->cullframe = g_state.frameCount;
        plane->cullside = SideCameraIsOn(plane);
    }

    return plane->cullside;
}

static void AddSurface(gl3_worldmodel_t *model, gl3_surface_t *surface, bool drawDecals = true, bool cullBackfaces = true)
{
    if (!surface->numverts)
    {
        return;
    }

    if (cullBackfaces && !(surface->flags & SURF_WATER) && gl3_brush_face_cull->value)
    {
        bool back = CacheSideCameraIsOn(surface->plane);
        bool invert = (surface->flags & SURF_BACK);
        if (back != invert)
        {
            return;
        }
    }

    if (surface->flags & SURF_WATER)
    {
        surface->texturechain = s_waterSurfaces;
        s_waterSurfaces = surface;
    }
    else if (surface->flags & SURF_SKY)
    {
        surface->texturechain = s_skySurfaces;
        s_skySurfaces = surface;
    }
    else
    {
        gl3_texture_t *texture = surface->texture;
        surface->texturechain = texture->texturechain;
        texture->texturechain = surface;
    }

    if (drawDecals)
    {
        decalAddFromSurface(model, surface);
    }
}

static void LinkLeafFaces(gl3_worldmodel_t *model, gl3_leaf_t &leaf)
{
    for (int j = 0; j < leaf.nummarksurfaces; j++)
    {
        gl3_surface_t *surface = leaf.firstmarksurface[j];
        if (surface->visframe == g_state.frameCount)
        {
            // already added
            continue;
        }

        surface->visframe = g_state.frameCount;
        AddSurface(model, surface);
    }
}

static void LinkLeaves(gl3_worldmodel_t *worldmodel)
{
    pvsUpdate(g_state.viewOrigin);

    for (int i = 0; i < g_pvsLeafCount; i++)
    {
        gl3_leaf_t *other = g_pvsLeaves[i];
        if (!g_state.viewFrustum.CullAABB(other->mins, other->maxs))
        {
            LinkLeafFaces(worldmodel, *other);
        }
    }
}


static float ScrollAmount(cl_entity_t *entity, gl3_surface_t *surface)
{
    if (!entity)
    {
        return 0;
    }

    if (!(surface->flags & SURF_SCROLL))
    {
        return 0;
    }

    float speed = (float)(entity->curstate.rendercolor.b + (entity->curstate.rendercolor.g << 8)) / 16.0f;
    if (!entity->curstate.rendercolor.r)
    {
        speed = -speed;
    }

    float scroll = 1.0f / (float)surface->texture->width * g_engfuncs.GetClientTime() * speed;
    if (scroll < 0)
    {
        return fmodf(scroll, -1);
    }
    else
    {
        return fmodf(scroll, 1);
    }
}

static void DrawSurfaces(gl3_worldmodel_t *worldmodel, cl_entity_t *entity, const BrushShader &shader, GLuint textureOverride)
{
    // index buffer is dynamic
    commandBindVertexBuffer(worldmodel->vertex_buffer, g_brushVertexFormat);

    float prevScroll = 0;
    commandUniform1f(shader.u_scroll, prevScroll);

    if (textureOverride)
    {
        commandBindTexture(0, GL_TEXTURE_2D, textureOverride);
    }

    for (int i = 0; i < worldmodel->numtextures; i++)
    {
        gl3_texture_t *texture = &worldmodel->textures[i];
        gl3_surface_t *chain = texture->texturechain;
        if (!chain)
        {
            continue;
        }

        texture->texturechain = nullptr;

        if (!textureOverride)
        {
            GLuint textureName = TextureAnimation(entity, texture)->gl_texturenum;
            commandBindTexture(0, GL_TEXTURE_2D, textureName);
        }

        for (gl3_surface_t *surface = chain; surface; surface = surface->texturechain)
        {
            float scroll = ScrollAmount(entity, surface);
            if (scroll != prevScroll)
            {
                DrawIndexBuffer();
                prevScroll = scroll;
                commandUniform1f(shader.u_scroll, prevScroll);
            }

            AddSurfaceToIndexBuffer(surface);
        }

        DrawIndexBuffer();
    }
}

static void DrawWaterSurfaces(cl_entity_t *entity, GLuint textureOverride)
{
    // checked and cleared by caller
    GL3_ASSERT(s_waterSurfaces);

    // restore the vertex buffer since drawing decals might have changed it
    commandBindVertexBuffer(g_worldmodel->vertex_buffer, g_brushVertexFormat);

    waterDrawBegin();

    GLuint prevTexture = textureOverride;
    commandBindTexture(0, GL_TEXTURE_2D, prevTexture);

    // very hacky!!!
    int textureIndex = s_waterSurfaces->texture - g_worldmodel->textures;
    g_state.waterColor = internalWaterColor(g_worldmodel->engine_model, textureIndex);

    for (gl3_surface_t *surface = s_waterSurfaces; surface; surface = surface->texturechain)
    {
        gl3_texture_t *texture = surface->texture;

        // shouldn't happen anymore
        GL3_ASSERT(texture->gl_texturenum);

        if (!textureOverride)
        {
            GLuint textureName = TextureAnimation(entity, texture)->gl_texturenum;
            if (textureName != prevTexture)
            {
                DrawIndexBuffer();
                prevTexture = textureName;
                commandBindTexture(0, GL_TEXTURE_2D, prevTexture);
            }
        }

        AddSurfaceToIndexBuffer(surface);
    }

    DrawIndexBuffer();

    waterDrawEnd();
}

static void DrawSkySurfaces()
{
    // checked and cleared by caller
    GL3_ASSERT(s_skySurfaces);

    if (!skyboxDrawBegin())
    {
        // nothing to draw
        return;
    }

    // restore the vertex buffer since drawing decals might have changed it
    commandBindVertexBuffer(g_worldmodel->vertex_buffer, g_brushVertexFormat);

    for (gl3_surface_t *surface = s_skySurfaces; surface; surface = surface->texturechain)
    {
        AddSurfaceToIndexBuffer(surface);
    }

    DrawIndexBuffer();

    skyboxDrawEnd();
}

static BrushShader &SelectShader(const BrushShaderOptions &options, bool alphaTest)
{
    if (!options.lightmapped)
    {
        return s_shaderNoLighting;
    }

    if (alphaTest)
    {
        return s_shaderAlphaTest;
    }

    return s_shader;
}

// draws all surfaces, decals, water and sky associated with this model
static void DrawAllSurfaces(gl3_worldmodel_t *worldmodel, cl_entity_t *entity, const BrushShaderOptions &options, GLuint textureOverride, bool alphaTest)
{
    BrushShader &shader = SelectShader(options, alphaTest);

    commandUseProgram(&shader);

    DrawSurfaces(worldmodel, entity, shader, textureOverride);

    // decal indices are stuffed into the same index buffer
    decalDrawAll(g_dynamicIndexState);

    if (s_waterSurfaces)
    {
        DrawWaterSurfaces(entity, textureOverride);
        s_waterSurfaces = nullptr;
    }

    if (s_skySurfaces)
    {
        DrawSkySurfaces();
        s_skySurfaces = nullptr;
    }
}

static void BrushModelBounds(cl_entity_t *entity, Vector3 &mins, Vector3 &maxs)
{
    model_t *model = entity->model;

    if (!VectorIsZero(entity->angles))
    {
        Vector3 radius{ model->radius, model->radius, model->radius };
        mins = entity->origin - radius;
        maxs = entity->origin + radius;
    }
    else
    {
        mins = entity->origin + model->mins;
        maxs = entity->origin + model->maxs;
    }
}

static bool CullBrushModel(cl_entity_t *entity)
{
    Vector3 mins, maxs;
    BrushModelBounds(entity, mins, maxs);
    return g_state.viewFrustum.CullAABB(mins, maxs);
}

static void LinkAndDrawBrushModel(gl3_worldmodel_t *worldmodel, cl_entity_t *entity, const BrushShaderOptions &options, bool alphaTest)
{
    // FIXME: this is assuming all brush models are inline models
    model_t *model = entity->model;

    // redundant computation (already done with bmodel cull)
    Vector3 mins, maxs;
    BrushModelBounds(entity, mins, maxs);

    // no decals on alpha tested surfaces
    bool decals = (entity->curstate.rendermode != kRenderTransAlpha);

    for (int i = 0; i < model->nummodelsurfaces; i++)
    {
        gl3_surface_t *surface = &worldmodel->surfaces[model->firstmodelsurface + i];
        gl3_plane_t *plane = surface->plane;

        // FIXME: confusing and ugly...
        if (surface->flags & SURF_WATER)
        {
            if (plane->type != PLANE_Z || mins.z + 1 >= plane->dist)
            {
                // get culled fuckass
                continue;
            }
        }

        // entity backfaces are not culled
        AddSurface(worldmodel, surface, decals, false);
    }

    // stupid hack for color rendermode
    GLuint textureOverride = 0;

    if (entity->curstate.rendermode == kRenderTransColor)
    {
        textureOverride = g_state.whiteTexture;
    }

    DrawAllSurfaces(worldmodel, entity, options, textureOverride, alphaTest);
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
    commandBindUniformBuffer(1, span.buffer, span.offset, sizeof(constants));
}

static void LinkAndDrawWorldModel(const BrushShaderOptions &options)
{
    SetupConstantBuffer(nullptr, { 1, 1, 1, 1 });

    // setup texturechains, water chain and decals
    LinkLeaves(g_worldmodel);

    DrawAllSurfaces(g_worldmodel, nullptr, options, 0, false);
}

void brushDrawSolids(
    cl_entity_t **entities,
    int entityCount,
    cl_entity_t **alphaEntities,
    int alphaEntityCount)
{
    MapIndexBuffer();

    // lightmap only used for solid brush entities
    commandBindTexture(1, GL_TEXTURE_2D, g_worldmodel->lightmap_texture);

    // draw fully opaque stuff
    {
        BrushShaderOptions options;
        options.lightmapped = true;

        LinkAndDrawWorldModel(options);

        for (int i = 0; i < entityCount; i++)
        {
            // FIXME: could cull earlier...
            if (CullBrushModel(entities[i]))
            {
                continue;
            }

            SetupConstantBuffer(entities[i], { 1, 1, 1, 1 });
            LinkAndDrawBrushModel(g_worldmodel, entities[i], options, false);
        }
    }

    // draw alpha tested stuff
    {
        BrushShaderOptions options;
        options.lightmapped = true;

        for (int i = 0; i < alphaEntityCount; i++)
        {
            // FIXME: could cull earlier...
            if (CullBrushModel(alphaEntities[i]))
            {
                continue;
            }

            SetupConstantBuffer(alphaEntities[i], { 1, 1, 1, 1 });
            LinkAndDrawBrushModel(g_worldmodel, alphaEntities[i], options, true);
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

    MapIndexBuffer();

    BrushShaderOptions options;
    options.lightmapped = false;

    Vector4 renderColor;
    SetBlendingAndGetColor(entity, renderColor, blend);

    SetupConstantBuffer(entity, renderColor);
    LinkAndDrawBrushModel(g_worldmodel, entity, options, false);

    UnmapIndexBuffer();
}

void brushEndTranslucents()
{
    // might leave blending enabled and depth mask disabled so check it here
    commandBlendEnable(GL_FALSE);
    commandDepthMask(GL_TRUE);
}

}
