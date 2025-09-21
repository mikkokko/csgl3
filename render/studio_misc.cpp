#include "stdafx.h"
#include "studio_misc.h"
#include "brush.h" // worldmodel lightmap
#include "internal.h"

namespace Render
{

inline Vector3 AnimatePoint(const Vector3 &point)
{
    const Vector4 *m = reinterpret_cast<Vector4 *>(g_engineStudio.StudioGetRotationMatrix());

    Vector3 result;
    result.x = Dot(reinterpret_cast<const Vector3 &>(m[0]), point) + m[0].w;
    result.y = Dot(reinterpret_cast<const Vector3 &>(m[1]), point) + m[1].w;
    result.z = Dot(reinterpret_cast<const Vector3 &>(m[2]), point) + m[2].w;
    return result;
}

bool studioFrustumCull(cl_entity_t *entity, studiohdr_t *header)
{
    Vector3 mins, maxs;

    if (!VectorIsZero(header->bbmin))
    {
        mins = entity->origin + header->bbmin;
        maxs = entity->origin + header->bbmax;
    }
    else if (!VectorIsZero(header->min))
    {
        mins = entity->origin + header->min;
        maxs = entity->origin + header->max;
    }
    else
    {
        mins = entity->origin + Vector3{ -16, -16, -16 };
        maxs = entity->origin + Vector3{ 16, 16, 16 };
    }

    if (entity->curstate.sequence >= header->numseq)
    {
        entity->curstate.sequence = 0;
    }

    mstudioseqdesc_t *sequences = (mstudioseqdesc_t *)((byte *)header + header->seqindex);
    mstudioseqdesc_t *sequence = &sequences[entity->curstate.sequence];

    for (int i = 0; i < 8; i++)
    {
        Vector3 point;
        point.x = (i & 1) ? sequence->bbmin.x : sequence->bbmax.x;
        point.y = (i & 2) ? sequence->bbmin.y : sequence->bbmax.y;
        point.z = (i & 4) ? sequence->bbmin.z : sequence->bbmax.z;

        point = AnimatePoint(point);

        mins.x = Q_min(mins.x, point.x);
        maxs.x = Q_max(maxs.x, point.x);

        mins.y = Q_min(mins.y, point.y);
        maxs.y = Q_max(maxs.y, point.y);

        mins.z = Q_min(mins.z, point.z);
        maxs.z = Q_max(maxs.z, point.z);
    }

    // can't do this! need to do a coarse check like the engine does
    //return g_state.viewFrustum.CullAABB(mins, maxs);

    gl3_plane_t plane{};
    plane.normal = g_state.viewForward;
    plane.dist = Dot(g_state.viewForward, g_state.viewOrigin);
    plane.type = PLANE_Y | PLANE_ANYY;
    return (BoxOnPlaneSide(mins, maxs, plane) == BopsBack);
}

void studioDynamicLight(cl_entity_t *entity, alight_t *light)
{
    // no filterMode handling (czero bloat?)

    if (r_fullbright->value == 1)
    {
        light->shadelight = 0;
        light->ambientlight = 192;
        light->color = { 1, 1, 1 };
        light->plightvec[0] = 0;
        light->plightvec[1] = 0;
        light->plightvec[2] = -1;
        return;
    }

    Vector3 direction;
    if (entity->curstate.effects & EF_INVLIGHT)
    {
        direction = { 0, 0, 1 };
    }
    else
    {
        direction = { 0, 0, -1 };
    }

    Vector3 start = entity->origin - direction * 8;

    colorVec sample{};

    if (g_state.movevars->skycolor_r + g_state.movevars->skycolor_g + g_state.movevars->skycolor_b)
    {
        Vector3 end;
        end.x = entity->origin.x - g_state.movevars->skyvec_x * 8192;
        end.y = entity->origin.y - g_state.movevars->skyvec_y * 8192;
        end.z = entity->origin.z - g_state.movevars->skyvec_z * 8192;

        // no constant for this flag in the sdk (it's in gl_model.h)
        if ((entity->model->flags & 1024) || internalTraceLineToSky(g_worldmodel->engine_model, start, end))
        {
            sample.r = (int)g_state.movevars->skycolor_r;
            sample.g = (int)g_state.movevars->skycolor_g;
            sample.b = (int)g_state.movevars->skycolor_b;
            direction.x = g_state.movevars->skyvec_x;
            direction.y = g_state.movevars->skyvec_y;
            direction.z = g_state.movevars->skyvec_z;
        }
    }

    if (!(sample.r + sample.g + sample.b))
    {
        Vector3 end = start + direction * 2048;
        sample = internalSampleLightmap(g_worldmodel->engine_model, start, end);

        start.x -= 16;
        start.y -= 16;
        end.x -= 16;
        end.y -= 16;
        colorVec sample2 = internalSampleLightmap(g_worldmodel->engine_model, start, end);
        float f1 = (float)(sample2.r + sample2.g + sample2.b) / 768.0f;

        start.x += 32;
        end.x += 32;
        sample2 = internalSampleLightmap(g_worldmodel->engine_model, start, end);
        float f2 = (float)(sample2.r + sample2.g + sample2.b) / 768.0f;

        start.y += 32;
        end.y += 32;
        sample2 = internalSampleLightmap(g_worldmodel->engine_model, start, end);
        float f3 = (float)(sample2.r + sample2.g + sample2.b) / 768.0f;

        start.x -= 32;
        end.x -= 32;
        sample2 = internalSampleLightmap(g_worldmodel->engine_model, start, end);
        float f4 = (float)(sample2.r + sample2.g + sample2.b) / 768.0f;

        direction.x = f4 - f2 - f3 + f1;
        direction.y = f2 - f3 - f4 + f1;
        VectorNormalize(direction);
    }

    if (entity->curstate.renderfx == kRenderFxLightMultiplier && entity->curstate.iuser4 != 10)
    {
        float scale = (float)entity->curstate.iuser4 / 10.0f;
        sample.r = (int)((float)sample.r * scale);
        sample.g = (int)((float)sample.g * scale);
        sample.b = (int)((float)sample.b * scale);
    }

    entity->cvFloorColor = sample;

    Vector3 color;
    color.x = (float)sample.r;
    color.y = (float)sample.g;
    color.z = (float)sample.b;

    float scale = static_cast<float>(Q_max(Q_max(sample.r, sample.g), sample.b));
    if (!scale)
    {
        scale = 1;
    }

    direction *= scale;

    for (int i = 0; i < MAX_DLIGHTS; i++)
    {
        dlight_t *dlight = &g_dlights[i];
        if (dlight->die < g_engfuncs.GetClientTime())
        {
            continue;
        }

        Vector3 delta = entity->origin - dlight->origin;
        float distance = VectorLength(delta);

        float contrib = dlight->radius - distance;
        if (contrib <= 0)
        {
            continue;
        }

        float deltaScale = (distance > 1) ? (contrib / distance) : contrib;
        delta *= deltaScale;
        direction += delta;

        scale += contrib;

        color.x += (float)dlight->color.r * contrib / 256.0f;
        color.y += (float)dlight->color.g * contrib / 256.0f;
        color.z += (float)dlight->color.b * contrib / 256.0f;
    }

    // again no constant for this flag
    if (entity->model->flags & 256)
    {
        direction *= 0.6f;
    }
    else
    {
        direction *= Q_clamp(v_direct->value, 0.75f, 1.0f);
    }

    light->shadelight = (int)VectorLength(direction);
    light->ambientlight = (int)scale - light->shadelight;

    scale = Q_max(Q_max(color.x, color.y), color.z);
    if (!scale)
    {
        light->color = { 1, 1, 1 };
    }
    else
    {
        light->color = color * (1.0f / scale);
    }

    if (light->ambientlight > 128)
    {
        light->ambientlight = 128;
    }

    if (light->ambientlight + light->shadelight > 255)
    {
        light->shadelight = 255 - light->ambientlight;
    }

    VectorNormalize(direction);
    light->plightvec[0] = direction.x;
    light->plightvec[1] = direction.y;
    light->plightvec[2] = direction.z;
}

studiohdr_t *studioTextureHeader(model_t *model, studiohdr_t *header)
{
    GL3_ASSERT(model);

    if (header->textureindex)
        return header;

    model_t *texmodel = (model_t *)model->texinfo;
    if (texmodel && g_engineStudio.Cache_Check(&texmodel->cache))
    {
        return (studiohdr_t *)texmodel->cache.data;
    }

    char path[64];
    size_t length = Q_strcpy(path, model->name);
    if (length < 4 || length + 1 >= sizeof(path))
    {
        // crash in caller code instead of here
        return header;
    }

    // dumb but this is how the engine does it...
    memcpy(path + length - 4, "t.mdl", 6);

    GL_ERRORS();
    texmodel = g_engineStudio.Mod_ForName(path, true);
    GL_ERRORS_QUIET();

    model->texinfo = (mtexinfo_t *)texmodel;

    // not sure why this is done but ok
    studiohdr_t *textureheader = (studiohdr_t *)texmodel->cache.data;
    Q_strcpy(textureheader->name, path);
    return textureheader;
}

}
