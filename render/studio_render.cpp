#include "stdafx.h"
#include "studio_render.h"
#include "studio_cache.h"
#include "studio_misc.h"
#include "studio_proxy.h"
#include "gamma.h"
#include "commandbuffer.h"
#include "dynamicbuffer.h"
#include "brush.h"

namespace Render
{

// must match shader
struct StudioConstants
{
    Vector4 renderColor;
    Vector4 lightDir;
    Vector4 ambientAndShadeLight; // x = ambientlight, y = shadelight
    Vector4 chromeOriginAndShellScale; // chrome origin (xyz) and glowshell scale (w)

    Vector4 elightPositions[STUDIO_MAX_ELIGHTS]; // 4th component stores radius^2
    Vector4 elightColors[STUDIO_MAX_ELIGHTS];

    // bones must be last! see StudioSetConstants
    Matrix3x4 bones[MAX_SHADER_BONES];
};

static const VertexAttrib s_vertexAttribs[] = {
    VERTEX_ATTRIB(StudioVertex, position),
    VERTEX_ATTRIB(StudioVertex, normal),
    VERTEX_ATTRIB(StudioVertex, texCoord),
#ifdef STUDIO_TANGENTS
    VERTEX_ATTRIB(StudioVertex, tangent),
#endif
    VERTEX_ATTRIB(StudioVertex, bone),
    VERTEX_ATTRIB_TERM()
};

const VertexFormat g_studioVertexFormat{ s_vertexAttribs, sizeof(StudioVertex) };

class StudioShader : public BaseShader
{
public:
    const char *Name()
    {
        return "studio";
    }

    const VertexAttrib *VertexAttribs()
    {
        return s_vertexAttribs;
    }

    const ShaderUniform *Uniforms()
    {
        static const ShaderUniform uniforms[] = {
            SHADER_UNIFORM_CONST(u_texture, 0),
            SHADER_UNIFORM_MUT(StudioShader, u_viewmodel),
            SHADER_UNIFORM_MUT(StudioShader, u_flags),
            SHADER_UNIFORM_TERM()
        };

        return uniforms;
    }

    GLint u_viewmodel;
    GLint u_flags;
};

static StudioShader s_shader;

class StudioShaderAlphaTest : public StudioShader
{
public:
    const char *Defines()
    {
        return "#define ALPHA_TEST 1\n";
    }
};

static StudioShaderAlphaTest s_shaderAlphaTest;

// cringe global state for shader selection
static bool s_viewmodel;
static StudioShader *s_currentShader;

static cvar_t *r_glowshellfreq;
static cvar_t *cl_righthand;

void studioRenderInit()
{
    shaderRegister(s_shader);
    shaderRegister(s_shaderAlphaTest);

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

    context.submodel = &submodels[model_index];
    context.rendererSubModel = &rendererBodypart->models[model_index];

    // set these for the game (most likely not used but just in case)
    *ppbodypart = bodypart;
    *ppsubmodel = context.submodel;
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

        context.elightPositions[index] = { elight->origin, radiusSquared };

        context.elightColors[index].x = g_gammaLinearTable[elight->color.r] * (1.0f / 255.0f);
        context.elightColors[index].y = g_gammaLinearTable[elight->color.g] * (1.0f / 255.0f);
        context.elightColors[index].z = g_gammaLinearTable[elight->color.b] * (1.0f / 255.0f);

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
    StudioConstants constants;

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

    GL3_ASSERT(context.header->numbones <= MAX_SHADER_BONES);

    constants.lightDir = { context.lightvec, 0 };
    constants.ambientAndShadeLight = { context.ambientlight, context.shadelight, 0, 0 };

    for (int i = 0; i < STUDIO_MAX_ELIGHTS; i++)
    {
        constants.elightPositions[i] = context.elightPositions[i];
        constants.elightColors[i] = { context.elightColors[i], 1.0f };
    }

    memcpy(static_cast<void *>(constants.bones), g_engineStudio.StudioGetBoneTransform(), sizeof(Matrix3x4) * context.header->numbones);

    constexpr int bonelessSize = sizeof(constants) - sizeof(constants.bones);
    int bonesSize = sizeof(Matrix3x4) * context.header->numbones;
    int constantsSize = bonelessSize + bonesSize;

    BufferSpan span = dynamicUniformData(&constants, constantsSize);
    commandBindUniformBuffer(1, span.buffer, span.offset, constantsSize);
}

void studioSetupRenderer(StudioContext &context, int rendermode)
{
    context.rendermode = rendermode;

    // set the model to be rendered
    commandBindVertexBuffer(context.cache->vertexBuffer, g_studioVertexFormat);
    commandBindIndexBuffer(context.cache->indexBuffer);

    // FIXME: bones uploaded twice for chromeshell
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

static int GetShaderFlags(StudioContext &context, int textureFlags)
{
    int shaderFlags = 0;

    if (textureFlags & STUDIO_NF_CHROME)
    {
        shaderFlags |= STUDIO_SHADER_CHROME;
    }

    if (context.rendermode == kRenderTransAdd
        || context.entity->curstate.renderfx == kRenderFxGlowShell)
    {
        shaderFlags |= STUDIO_SHADER_COLOR_ONLY;
    }
    else
    {
        if (textureFlags & STUDIO_NF_FLATSHADE)
        {
            shaderFlags |= STUDIO_SHADER_FLATSHADE;
        }

        if (textureFlags & STUDIO_NF_FULLBRIGHT)
        {
            shaderFlags |= STUDIO_SHADER_FULLBRIGHT;
        }
    }

    if (context.elightCount > 0)
    {
        shaderFlags |= STUDIO_SHADER_ELIGHTS;
    }

    return shaderFlags;
}

// selects and uses the correct shader program, sets uniforms on the default block
static void StudioUseProgram(StudioContext &context, int textureFlags)
{
    bool alphaTest = (textureFlags & STUDIO_NF_MASKED);
    StudioShader *shader = alphaTest ? &s_shaderAlphaTest : &s_shader;
    if (shader != s_currentShader)
    {
        s_currentShader = shader;
        commandUseProgram(s_currentShader);
        commandUniform1i(s_currentShader->u_viewmodel, s_viewmodel);
    }

    // update the flags uniform every time
    commandUniform1i(shader->u_flags, GetShaderFlags(context, textureFlags));
}

void studioDrawPoints(StudioContext &context)
{
    studiohdr_t *header = context.header;
    studiohdr_t *textureheader = studioTextureHeader(context.model, header);

    mstudiomodel_t *submodel = context.submodel;
    StudioSubModel *mem_submodel = context.rendererSubModel;

    mstudiomesh_t *meshes = (mstudiomesh_t *)((byte *)header + submodel->meshindex);
    mstudiotexture_t *textures = (mstudiotexture_t *)((byte *)textureheader + textureheader->textureindex);

    short *skins = (short *)((byte *)textureheader + textureheader->skinindex);
    int skin = context.entity->curstate.skin;

    if (skin && skin < textureheader->numskinfamilies)
    {
        skins = &skins[skin * textureheader->numskinref];
    }

    int indexSize = context.cache->indexSize;
    GLenum indexType = (indexSize == 4) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

    for (int i = 0; i < submodel->nummesh; i++)
    {
        mstudiomesh_t *mesh = &meshes[i];
        mstudiotexture_t *texture = &textures[skins[mesh->skinref]];
        StudioMesh *mem_mesh = &mem_submodel->meshes[i];

        bool additive = ((texture->flags & STUDIO_NF_ADDITIVE) && context.entity->curstate.rendermode == kRenderNormal);
        if (additive)
        {
            commandBlendEnable(GL_TRUE);
            commandBlendFunc(GL_ONE, GL_ONE);
            commandDepthMask(GL_FALSE);
        }

        StudioUseProgram(context, texture->flags | g_engineStudio.GetForceFaceFlags());

        // FIXME: remaps won't work!!! we could have called StudioSetupSkin,
        // but now we have the command buffer system going on...
        if ((g_engineStudio.GetForceFaceFlags() & STUDIO_NF_CHROME) == 0)
        {
            commandBindTexture(0, GL_TEXTURE_2D, texture->index);
        }

        commandDrawElements(GL_TRIANGLES, mem_mesh->indexCount, indexType, mem_mesh->indexOffset_notbytes * indexSize);

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
    GL3_ASSERT(!s_currentShader);
}

void studioEndModels()
{
    s_currentShader = nullptr;
}

}
