#include "stdafx.h"
#include "studio_render.h"
#include "studio_cache.h"
#include "studio_misc.h"
#include "studio_proxy.h"
#include "gamma.h"
#include "commandbuffer.h"
#include "brush.h"

namespace Render
{

// must match shader
struct StudioConstants
{
    Vector4 renderColor;
    Vector4 renderColorLinear; // only used for elights
    Vector4 lightDir;
    Vector4 ambientAndShadeLight; // x = ambientlight, y = shadelight
    Vector4 chromeOriginAndShellScale; // chrome origin (xyz) and glowshell scale (w)

    // room for 4 lights, but only 3 used
    float elights[3][4]; // x,y,z
    float elightColors[3][4]; // r,g,b

    // boolean constants...
    int viewmodel;
    int additive;
    int padding[2];
};

struct StudioTextureConstants
{
    // only used for glowshell
    Vector2 uvScale;

    // not used for glowshell
    // separate bools for GLSL 1.20
    int fullbright;
    int flatshade;
    int chrome;
    int padding;
};

static const VertexAttrib s_vertexAttribs[] = {
    { &StudioVertex::position, "a_position" },
    { &StudioVertex::texCoord, "a_texCoord" },
    { &StudioVertex::bone, "a_bone" },
    { &StudioVertex::normal, "a_normal", true },
    { &StudioVertex::smoothNormal, "a_smoothNormal", true }
};

const VertexFormat g_studioVertexFormat{ sizeof(StudioVertex), s_vertexAttribs };

static const ShaderUniform s_uniforms[] = {
    { "u_texture", 0 }
};

static constexpr ShaderOption s_shaderOptions[] = {
    { "ALPHA_TEST", 1 },
    { "HAS_ELIGHTS", 1 }
};

// must match s_shaderOptions
struct StudioShaderOptions
{
    unsigned alphaTest;
    unsigned hasElights;
};

static BaseShader s_shaders[shaderVariantCount(s_shaderOptions)];

// just use the same shader struct...
static BaseShader s_shaderGlowShell;

// cringe global state for shader selection
static bool s_viewmodel;

static cvar_t *r_glowshellfreq;
static cvar_t *cl_righthand;

void studioRenderInit()
{
    shaderRegister(s_shaders, "studio", s_vertexAttribs, s_uniforms, s_shaderOptions);
    shaderRegister(s_shaderGlowShell, "studio_glowshell", s_vertexAttribs, s_uniforms);

    r_glowshellfreq = g_engfuncs.pfnGetCvarPointer("r_glowshellfreq");
    cl_righthand = g_engfuncs.pfnGetCvarPointer("cl_righthand");
}

template<typename T>
T *StudioGet(void *base, int offset)
{
    return (T *)((byte *)base + offset);
}

void studioSetupModel(StudioContext &context, int bodypartIndex, mstudiobodyparts_t **ppbodypart, mstudiomodel_t **ppsubmodel)
{
    if (bodypartIndex > context.header->numbodyparts)
        bodypartIndex = 0;

    mstudiobodyparts_t *bodyparts = StudioGet<mstudiobodyparts_t>(context.header, context.header->bodypartindex);
    mstudiobodyparts_t *bodypart = &bodyparts[bodypartIndex];
    mstudiomodel_t *submodels = StudioGet<mstudiomodel_t>(context.header, bodypart->modelindex);

    StudioBodypart *rendererBodypart = &context.cache->bodyparts[bodypartIndex];

    int model_index = (context.entity->curstate.body / bodypart->base) % bodypart->nummodels;

    context.rendererSubModel = &rendererBodypart->models[model_index];

    // set these for the game (most likely not used but just in case)
    *ppbodypart = bodypart;
    *ppsubmodel = &submodels[model_index];
}

void studioEntityLight(StudioContext &context)
{
    context.elightCount = 0;
    memset(context.elightColors, 0, sizeof(context.elightColors));
    memset(context.elightPositions, 0, sizeof(context.elightPositions));

    float strengths[STUDIO_MAX_ELIGHTS]{};

    float max_radius = 1000000;
    float min_radius = 0;

    cl_entity_t *entity = context.entity;

    for (int i = 0; i < MAX_ELIGHTS; i++)
    {
        dlight_t *elight = &g_elights[i];

        if (elight->die <= g_engfuncs.GetClientTime())
        {
            continue;
        }

        if (elight->radius <= min_radius)
        {
            continue;
        }

        if ((elight->key & 0xFFF) == entity->index)
        {
            int attachment = (elight->key >> 12) & 0xF;
            GL3_ASSERT(attachment >= 0 && attachment < 4);

            if (attachment)
            {
                elight->origin = entity->attachment[attachment];
            }
            else
            {
                elight->origin = entity->origin;
            }
        }

        Vector3 direction = entity->origin - elight->origin;
        float distanceSquared = Dot(direction, direction);

        float radiusSquared = elight->radius * elight->radius;

        float strength;

        if (distanceSquared <= radiusSquared)
        {
            strength = 1;
        }
        else
        {
            strength = radiusSquared / distanceSquared;
            if (strength <= 0.004f)
            {
                continue;
            }
        }

        int index = context.elightCount;

        if (context.elightCount >= STUDIO_MAX_ELIGHTS)
        {
            index = -1;

            for (int j = 0; j < context.elightCount; j++)
            {
                if (strengths[j] < max_radius && strengths[j] < strength)
                {
                    index = j;
                    max_radius = strengths[j];
                }
            }
        }

        if (index == -1)
        {
            continue;
        }

        strengths[index] = strength;

        context.elightPositions[index] = elight->origin;

        context.elightColors[index].x = g_gammaLinearTable[elight->color.r] * (1.0f / 255.0f) * radiusSquared;
        context.elightColors[index].y = g_gammaLinearTable[elight->color.g] * (1.0f / 255.0f) * radiusSquared;
        context.elightColors[index].z = g_gammaLinearTable[elight->color.b] * (1.0f / 255.0f) * radiusSquared;

        if (index >= context.elightCount)
        {
            context.elightCount = index + 1;
        }
    }
}

void studioSetupLighting(StudioContext &context, const alight_t *lighting)
{
    // store in context, will get copied to the constant buffer in studioSetupRenderer
    context.ambientlight = static_cast<float>(lighting->ambientlight) * (1.0f / 255.0f);
    context.shadelight = static_cast<float>(lighting->shadelight) * (1.0f / 255.0f);
    context.lightcolor = lighting->color;
    context.lightvec = { lighting->plightvec[0], lighting->plightvec[1], lighting->plightvec[2] };
}

static void StudioSetConstants(StudioContext &context)
{
    StudioConstants constants{};

    entity_state_t &state = context.entity->curstate;
    if (state.renderfx == kRenderFxGlowShell)
    {
        // glowshell specific
        float offset = r_glowshellfreq->value * g_engfuncs.GetClientTime();
        constants.chromeOriginAndShellScale.x = cosf(offset) * 4000.0f;
        constants.chromeOriginAndShellScale.y = sinf(offset) * 4000.0f;
        constants.chromeOriginAndShellScale.z = cosf(offset * 0.33f) * 4000.0f;
        constants.chromeOriginAndShellScale.w = static_cast<float>(state.renderamt) * 0.05f;

        constants.renderColor.x = state.rendercolor.r * (1.0f / 255);
        constants.renderColor.y = state.rendercolor.g * (1.0f / 255);
        constants.renderColor.z = state.rendercolor.b * (1.0f / 255);
        constants.renderColor.w = 1.0f;
    }
    else
    {
        if (context.rendermode == kRenderTransAdd)
        {
            constants.renderColor = { context.blend, context.blend, context.blend, 1 };
        }
        else
        {
            constants.renderColor = { context.lightcolor, context.blend };
        }

        // no shell effect
        constants.chromeOriginAndShellScale = { g_state.viewOrigin, 0.0f };
    }

    // this is for the elights...
    if (context.elightCount > 0)
    {
        constants.renderColorLinear.x = powf(constants.renderColor.x, g_gamma);
        constants.renderColorLinear.y = powf(constants.renderColor.y, g_gamma);
        constants.renderColorLinear.z = powf(constants.renderColor.z, g_gamma);
        constants.renderColorLinear.w = 0.0f;
    }

    GL3_ASSERT(context.header->numbones <= MAXSTUDIOBONES);

    constants.lightDir = { context.lightvec, 0 };
    constants.ambientAndShadeLight = { context.ambientlight, context.shadelight, 0, 0 };

    // the shader assumes this
    static_assert(STUDIO_MAX_ELIGHTS == 3, "bruh");

    for (int i = 0; i < STUDIO_MAX_ELIGHTS; i++)
    {
        constants.elights[0][i] = context.elightPositions[i].x;
        constants.elights[1][i] = context.elightPositions[i].y;
        constants.elights[2][i] = context.elightPositions[i].z;
        constants.elightColors[0][i] = context.elightColors[i].x;
        constants.elightColors[1][i] = context.elightColors[i].y;
        constants.elightColors[2][i] = context.elightColors[i].z;
    }

    constants.viewmodel = s_viewmodel;
    constants.additive = (context.rendermode == kRenderTransAdd);

    commandSetUniformData(UniformBlock::Model, &constants, sizeof(constants));
}

static void StudioSetBonePalette(StudioContext &context, int paletteIndex)
{
    GL3_ASSERT(paletteIndex >= 0 && paletteIndex < context.cache->paletteCount);
    const StudioBonePalette &palette = context.cache->palettes[paletteIndex];

    Matrix3x4 *boneMatrices = reinterpret_cast<Matrix3x4 *>(g_engineStudio.StudioGetBoneTransform());

    Matrix3x4 constants[MAXSTUDIOBONES];
    for (int i = 0; i < palette.boneCount; i++)
    {
        constants[i] = boneMatrices[palette.bones[i]];
    }

    int constantsSize = sizeof(Matrix3x4) * palette.boneCount;

    commandSetUniformData(UniformBlock::Bone, constants, constantsSize);

    context.bonePalette = paletteIndex;
}

void studioSetupRenderer(StudioContext &context, int rendermode)
{
    context.rendermode = rendermode;

    // set the model to be rendered (vertex buffer set in studioDrawPoints)
    commandBindIndexBuffer(context.cache->indexBuffer);

    StudioSetConstants(context);

    // set the rendermode here too
    if (rendermode != kRenderNormal)
    {
        commandBlendEnable(GL_TRUE);

        if (rendermode == kRenderTransAdd)
        {
            commandBlendFunc(GL_ONE, GL_ONE);
            commandDepthMask(GL_FALSE);
        }
        else
        {
            commandBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    if (s_viewmodel && cl_righthand && cl_righthand->value)
    {
        // disable culling for flipped viewmodels
        commandCullFace(GL_FALSE);
    }
}

void studioRestoreRenderer(StudioContext &context)
{
    // restore blending and depth mask i guess
    if (context.rendermode != kRenderNormal)
    {
        commandBlendEnable(GL_FALSE);

        if (context.rendermode == kRenderTransAdd)
        {
            commandDepthMask(GL_TRUE);
        }
    }

    if (s_viewmodel && cl_righthand && cl_righthand->value)
    {
        commandCullFace(GL_TRUE);
    }
}

// selects and uses the correct shader program, sets uniforms on the default block
static void StudioUseProgram(StudioContext &context, mstudiotexture_t *texture, int textureFlags)
{
    BaseShader *shader;

    bool glowShell = (context.entity->curstate.renderfx == kRenderFxGlowShell);
    if (glowShell)
    {
        shader = &s_shaderGlowShell;
    }
    else
    {
        StudioShaderOptions options{};
        options.alphaTest = (textureFlags & STUDIO_NF_MASKED) ? 1 : 0;
        options.hasElights = (context.elightCount > 0) ? 1 : 0;
        shader = &shaderSelect(s_shaders, s_shaderOptions, options);
    }

    commandUseProgram(shader);

    StudioTextureConstants constants{};

    if (glowShell)
    {
        // glowshell texture scale to match the engine look
        // FIXME: this is not optimal.. should revisit and decide what to do
        float xScale = 64.0f / static_cast<float>(texture->width);
        float yScale = 64.0f / static_cast<float>(texture->height);
        constants.uvScale = { xScale, yScale };
    }
    else
    {
        constants.fullbright = (textureFlags & STUDIO_NF_FULLBRIGHT) != 0;
        constants.flatshade = (textureFlags & STUDIO_NF_FLATSHADE) != 0;
        constants.chrome = (textureFlags & STUDIO_NF_CHROME) != 0;
    }

    commandSetUniformData(UniformBlock::Texture, &constants, sizeof(constants));
}

static bool IsRemapSkin(const char *texture)
{
    if (!Q_strcasecmp(texture, "DM_Base.bmp"))
    {
        // old system
        return true;
    }

    if (Q_strcasecmp(texture, "Remap", 5))
    {
        // not remapped
        return false;
    }

    size_t length = strlen(texture);
    if (length == 18)
    {
        // short form REMAPC_???_???.BMP
        return (texture[5] == 'c' || texture[5] == 'C');
    }

    // long form REMAP?_???_???_???.BMP
    return (length == 22);
}

static bool SetupSkin(cl_entity_t *entity, studiohdr_t *textureheader, mstudiotexture_t *texture, int textureIndex)
{
    // avoid calling StudioSetupSkin by any means necessary
    if (entity->index <= 0 || !IsRemapSkin(texture->name))
    {
        return false;
    }

    // this will do all of the fucked up stuff and eventually call glBindTexture
    g_engineStudio.StudioSetupSkin(textureheader, textureIndex);

#ifdef SCHIZO_DEBUG
    GLint activeTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    GL3_ASSERT(activeTexture == GL_TEXTURE0);
#endif

    // get the right texture from the driver shadow state
    // common sense would suggest that this wouldn't cause any issues, but i guess we'll see...
    GLint textureName;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureName);
    commandBindTexture(0, GL_TEXTURE_2D, textureName);
    return true;
}

void studioDrawPoints(StudioContext &context)
{
    studiohdr_t *header = context.header;
    studiohdr_t *textureheader = studioTextureHeader(context.model, header);

    StudioSubModel *mem_submodel = context.rendererSubModel;

    mstudiotexture_t *textures = (mstudiotexture_t *)((byte *)textureheader + textureheader->textureindex);

    short *skins = (short *)((byte *)textureheader + textureheader->skinindex);
    int skin = context.entity->curstate.skin;

    if (skin && skin < textureheader->numskinfamilies)
    {
        skins = &skins[skin * textureheader->numskinref];
    }

    for (int i = 0; i < mem_submodel->subMeshCount; i++)
    {
        StudioSubMesh *mem_mesh = &mem_submodel->subMeshes[i];
        int textureIndex = skins[mem_mesh->skinref];
        mstudiotexture_t *texture = &textures[textureIndex];

        if (mem_mesh->bonePalette != context.bonePalette)
        {
            StudioSetBonePalette(context, mem_mesh->bonePalette);
        }

        bool additive = ((texture->flags & STUDIO_NF_ADDITIVE) && context.entity->curstate.rendermode == kRenderNormal);
        if (additive)
        {
            commandBlendEnable(GL_TRUE);
            commandBlendFunc(GL_ONE, GL_ONE);
            commandDepthMask(GL_FALSE);
        }

        StudioUseProgram(context, texture, texture->flags | g_engineStudio.GetForceFaceFlags());

        // FIXME: remaps won't work!!! we could have called StudioSetupSkin,
        // but now we have the command buffer system going on...
        if ((g_engineStudio.GetForceFaceFlags() & STUDIO_NF_CHROME) == 0)
        {
            if (!SetupSkin(context.entity, textureheader, texture, textureIndex))
            {
                commandBindTexture(0, GL_TEXTURE_2D, texture->index);
            }
        }

        commandBindVertexBuffer(context.cache->vertexBuffer, g_studioVertexFormat, mem_mesh->baseVertex);

        commandDrawElements(GL_TRIANGLES,
            mem_mesh->indexCount,
            GL_UNSIGNED_SHORT,
            mem_mesh->indexOffsetInBytes);

        if (additive)
        {
            commandBlendEnable(GL_FALSE);
            commandDepthMask(GL_TRUE);
        }
    }
}

void studioBeginModels(bool viewmodel)
{
    s_viewmodel = viewmodel;
}

void studioEndModels()
{
    // nothing
}

}
