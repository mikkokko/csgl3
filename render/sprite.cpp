#include "stdafx.h"
#include "sprite.h"
#include "immediate.h"
#include "internal.h"

// not all sdks come with spritegn.h so define these here
#define SPR_VP_PARALLEL_UPRIGHT 0
#define SPR_FACING_UPRIGHT 1
#define SPR_VP_PARALLEL 2
#define SPR_ORIENTED 3
#define SPR_VP_PARALLEL_ORIENTED 4

namespace Render
{

static bool s_alphaBlend;
static int s_rendermode = -1;

static cvar_t *r_traceglow;

void spriteInit()
{
    r_traceglow = g_engfuncs.pfnGetCvarPointer("r_traceglow");
}

void spriteBegin(bool alphaBlend)
{
    s_rendermode = -1;
    s_alphaBlend = alphaBlend;

    immediateDrawStart(true);
    immediateBlendEnable(alphaBlend);
}

void spriteEnd()
{
    immediateDrawEnd();
}

static Vector4 SpriteColor(const cl_entity_t *entity, float blend)
{
    Vector4 result;

    float scale = 1;
    if (entity->curstate.rendermode == kRenderGlow || entity->curstate.rendermode == kRenderTransAdd)
    {
        scale = blend;
    }

    if (entity->curstate.rendercolor.r || entity->curstate.rendercolor.g || entity->curstate.rendercolor.b)
    {
        result.x = (entity->curstate.rendercolor.r * scale) / 256.0f;
        result.y = (entity->curstate.rendercolor.g * scale) / 256.0f;
        result.z = (entity->curstate.rendercolor.b * scale) / 256.0f;
    }
    else
    {
        result.x = (255 * scale) / 256.0f;
        result.y = (255 * scale) / 256.0f;
        result.z = (255 * scale) / 256.0f;
    }

    switch (entity->curstate.rendermode)
    {
    case kRenderGlow:
    case kRenderTransAdd:
        result.w = 1.0f;
        break;

    default:
        result.w = blend;
        break;
    }

    return result;
}

static bool BillboardSprite(const cl_entity_t *entity, int type, Vector3 &right, Vector3 &up)
{
    if (type == SPR_VP_PARALLEL && entity->angles.z != 0.0f)
    {
        type = SPR_VP_PARALLEL_ORIENTED;
    }

    // cos(radians(1))
    const float uprightLimit = 0.99984769515639123916f;

    switch (type)
    {
    case SPR_VP_PARALLEL_UPRIGHT:
        if (g_state.viewForward.z > uprightLimit || g_state.viewForward.z < -uprightLimit)
        {
            return false;
        }
        right = { g_state.viewForward.y, -g_state.viewForward.x, 0 };
        VectorNormalize(right);
        up = { 0, 0, 1 };
        break;

    case SPR_FACING_UPRIGHT:
    {
        Vector3 forward = -g_state.viewOrigin;
        VectorNormalize(forward);
        if (forward.z > uprightLimit || forward.z < -uprightLimit)
        {
            return false;
        }
        right = { forward.y, -forward.x, 0 };
        VectorNormalize(right);
        up = { 0, 0, 1 };
        break;
    }

    case SPR_VP_PARALLEL:
        right = g_state.viewRight;
        up = g_state.viewUp;
        break;

    case SPR_ORIENTED:
        AngleVectors(entity->angles, nullptr, &right, &up);
        break;

    case SPR_VP_PARALLEL_ORIENTED:
    {
        float angle = Radians(entity->angles.z);
        float sin = sinf(angle);
        float cos = cosf(angle);
        right = g_state.viewRight * cos + g_state.viewUp * sin;
        up = g_state.viewRight * -sin + g_state.viewUp * cos;
        break;
    }

    default:
        GL3_ASSERT(false);
        return false;
    }

    return true;
}

static float GlowBlend(cl_entity_t *entity, Vector3 &origin, bool traceglow)
{
    float distance = VectorLength(origin - g_state.viewOrigin);

    int traceFlags = PM_GLASS_IGNORE;
    if (!traceglow)
    {
        traceFlags |= PM_STUDIO_IGNORE;
    }

    pmtrace_t trace;
    g_engfuncs.pEventAPI->EV_SetTraceHull(2);
    g_engfuncs.pEventAPI->EV_PlayerTrace(&g_state.viewOrigin.x, &origin.x, traceFlags, -1, &trace);

    if ((1 - trace.fraction) * distance > 8)
    {
        return 0;
    }

    if (entity->curstate.renderfx == kRenderFxNoDissipation)
    {
        return static_cast<float>(entity->curstate.renderamt) / 255.0f;
    }

    entity->curstate.scale = distance * 0.005f;

    return Q_clamp(19000.0f / (distance * distance), 0.05f, 1.0f);
}

static void SetRendermode(int rendermode)
{
    if (!s_alphaBlend || s_rendermode == rendermode)
    {
        return;
    }

    s_rendermode = rendermode;

    switch (rendermode)
    {
    case kRenderTransAdd:
        immediateBlendFunc(GL_ONE, GL_ONE);
        immediateDepthTest(GL_TRUE);
        immediateDepthMask(GL_FALSE);
        break;

    case kRenderGlow:
        immediateBlendFunc(GL_ONE, GL_ONE);
        immediateDepthTest(GL_FALSE);
        immediateDepthMask(GL_FALSE);
        break;

    case kRenderTransAlpha:
        immediateBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        immediateDepthTest(GL_TRUE);
        immediateDepthMask(GL_FALSE);
        break;

    default:
        immediateBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        immediateDepthTest(GL_TRUE);
        immediateDepthMask(GL_TRUE);
        break;
    }
}

// wrapper for TexCoord2f and Vertex3f
static void EmitVertex(const Vector3 &position, const Vector2 &texCoord)
{
    immediateTexCoord2f(texCoord.x, texCoord.y);
    immediateVertex3f(position.x, position.y, position.z);
}

static void GetAttachmentPoint(Vector3 &point, int entityIndex, int attachmentIndex)
{
    cl_entity_t *entity = g_engfuncs.GetEntityByIndex(entityIndex);
    if (!entity)
    {
        return;
    }

    if (attachmentIndex)
    {
        point = entity->attachment[attachmentIndex - 1];
    }
    else
    {
        point = entity->origin;
    }
}

void spriteDraw(cl_entity_t *entity, float blend)
{
    SpriteInfo sprite;
    if (!internalGetSpriteInfo(entity->model, (int)entity->curstate.frame, &sprite))
    {
        return;
    }

    Vector3 origin = entity->origin;

    if (entity->curstate.body)
    {
        GetAttachmentPoint(origin, entity->curstate.skin, entity->curstate.body);
    }

    if (entity->curstate.rendermode == kRenderGlow)
    {
        // GlowBlend modifies entity->curstate.scale because fuck you
        blend *= GlowBlend(entity, origin, r_traceglow->value ? true : false);

        if (blend <= 0)
        {
            // not visible
            return;
        }
    }

    float scale = (entity->curstate.scale > 0) ? entity->curstate.scale : 1;

    // sloppy sphere cull
    {
        float max_x = Q_max(fabsf(sprite.left), fabsf(sprite.right)) * scale;
        float max_y = Q_max(fabsf(sprite.up), fabsf(sprite.down)) * scale;
        float radius = sqrtf(max_x * max_x + max_y * max_y);

        if (g_state.viewFrustum.CullSphere(origin, radius))
        {
            // get culled idiot
            return;
        }
    }

    float scale_up = sprite.up * scale;
    float scale_down = sprite.down * scale;
    float scale_left = sprite.left * scale;
    float scale_right = sprite.right * scale;

    Vector3 right, up;
    if (!BillboardSprite(entity, sprite.type, right, up))
    {
        // not visible
        return;
    }

    Vector3 vertices[4] = {
        origin + (up * scale_down) + (right * scale_left),
        origin + (up * scale_up) + (right * scale_left),
        origin + (up * scale_up) + (right * scale_right),
        origin + (up * scale_down) + (right * scale_right)
    };

    Vector4 color = SpriteColor(entity, blend);

    SetRendermode(entity->curstate.rendermode);

    immediateBindTexture(sprite.texture);
    immediateColor4f(color.x, color.y, color.z, color.w);

    immediateBegin(GL_QUADS);
    EmitVertex(vertices[0], { 0, 1 });
    EmitVertex(vertices[1], { 0, 0 });
    EmitVertex(vertices[2], { 1, 0 });
    EmitVertex(vertices[3], { 1, 1 });
    immediateEnd();
}

}
