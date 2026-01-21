#include "stdafx.h"
#include "studio_misc.h"
#include "brush.h" // worldmodel lightmap
#include "internal.h"
#include "lightstyle.h"

namespace Render
{

// FIXME: dumb
constexpr int MaxClientEntities = 1024;

// in large levels, sampling lightmaps is relatively slow, especially now with bilinear filtering
// in levels that use studio models as static props, it's obviously a complete waste of time
// to recompute lighting every frame... to solve this i added a per entity lighting data cache
// that only gets recomputed when the lighting conditions change

enum LightingType
{
    LightingNormal,
    LightingSky
};

struct NormalLighting
{
    LightmapSamples sample;
    LightmapSamples intensity[4];
};

struct SkyLighting
{
    Vector3 color;
};

struct LightingData
{
    LightingType type;

    union
    {
        NormalLighting normal;
        SkyLighting sky;
    } lighting;
};

struct LightingKey
{
    Vector3 origin;
    uint16_t effects;
    uint16_t model_flags;
};

struct LightingCache
{
    bool dirty{ true };

    LightingKey key;
    LightingData data;
    Vector3 direction;
};

// updated from movevars
static Vector3 s_skyColor;
static Vector3 s_skyVec;

static LightingCache s_lightingCache[MaxClientEntities]{};

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
    if (entity == g_engfuncs.GetViewModel())
    {
        // no need to cull, and interferes with bbox visualization
        return false;
    }

    if (entity->curstate.sequence >= header->numseq)
    {
        entity->curstate.sequence = 0;
    }

    mstudioseqdesc_t *sequences = (mstudioseqdesc_t *)((byte *)header + header->seqindex);
    mstudioseqdesc_t *sequence = &sequences[entity->curstate.sequence];

    Vector3 localMins, localMaxs;

    // none of the fallback paths are realistically executed, but keep them in anyway
    if (!VectorIsZero(sequence->bbmin))
    {
        localMins = sequence->bbmin;
        localMaxs = sequence->bbmax;
    }
    else if (!VectorIsZero(header->bbmin))
    {
        localMins = header->bbmin;
        localMaxs = header->bbmax;
    }
    else if (!VectorIsZero(header->min))
    {
        localMins = header->min;
        localMaxs = header->max;
    }
    else
    {
        localMins = { -16, -16, -16 };
        localMaxs = { 16, 16, 16 };
    }

    Vector3 mins{ FLT_MAX, FLT_MAX, FLT_MAX };
    Vector3 maxs{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (int i = 0; i < 8; i++)
    {
        Vector3 point;
        point.x = (i & 1) ? localMins.x : localMaxs.x;
        point.y = (i & 2) ? localMins.y : localMaxs.y;
        point.z = (i & 4) ? localMins.z : localMaxs.z;

        Vector3 worldPoint = AnimatePoint(point);

        mins.x = Q_min(mins.x, worldPoint.x);
        maxs.x = Q_max(maxs.x, worldPoint.x);

        mins.y = Q_min(mins.y, worldPoint.y);
        maxs.y = Q_max(maxs.y, worldPoint.y);

        mins.z = Q_min(mins.z, worldPoint.z);
        maxs.z = Q_max(maxs.z, worldPoint.z);
    }

    Vector3 center = (mins + maxs) * 0.5f;
    Vector3 extents = (maxs - mins) * 0.5f;

    return g_state.viewFrustum.CullBox(center, extents);
}

void studioUpdateSkyLight(const movevars_t *mv)
{
    Vector3 skyColor{ mv->skycolor_r, mv->skycolor_g, mv->skycolor_b };
    Vector3 skyVec{ mv->skyvec_x, mv->skyvec_y, mv->skyvec_z };

    if (s_skyColor == skyColor && s_skyVec == skyVec)
    {
        // no change
        return;
    }

    s_skyColor = skyColor;
    s_skyVec = skyVec;

    // invalidate skylight caches
    for (LightingCache &cache : s_lightingCache)
    {
        if (!cache.dirty && cache.data.type == LightingSky)
        {
            cache.dirty = true;
        }
    }
}

static void ComputeLightingForKey(Vector3 &direction, LightingData &result, const LightingKey &key)
{
    if (key.effects & EF_INVLIGHT)
    {
        direction = { 0, 0, 1 };
    }
    else
    {
        direction = { 0, 0, -1 };
    }

    Vector3 start = key.origin - direction * 8;

    if (!VectorIsZero(s_skyColor))
    {
        Vector3 end = key.origin - (s_skyVec * 8192);

        // no constant for this flag in the sdk (it's in gl_model.h)
        if ((key.model_flags & 1024) || internalTraceLineToSky(g_worldmodel->engine_model, start, end))
        {
            result.type = LightingSky;
            result.lighting.sky.color = s_skyColor;
            direction = s_skyVec;
            return;
        }
    }

    result.type = LightingNormal;

    // need to do this bullshit
    Vector3 end = start + direction * 2048;
    result.lighting.normal.sample = internalSampleLightmap(g_worldmodel->engine_model, start, end);

    start.x -= 16;
    start.y -= 16;
    end.x -= 16;
    end.y -= 16;
    result.lighting.normal.intensity[0] = internalSampleLightmap(g_worldmodel->engine_model, start, end);

    start.x += 32;
    end.x += 32;
    result.lighting.normal.intensity[1] = internalSampleLightmap(g_worldmodel->engine_model, start, end);

    start.y += 32;
    end.y += 32;
    result.lighting.normal.intensity[2] = internalSampleLightmap(g_worldmodel->engine_model, start, end);

    start.x -= 32;
    end.x -= 32;
    result.lighting.normal.intensity[3] = internalSampleLightmap(g_worldmodel->engine_model, start, end);
}

static const LightingData &GetCachedLightingData(Vector3 &out_direction, cl_entity_t *entity)
{
    LightingKey key;
    key.origin = entity->origin;
    key.effects = (uint16_t)entity->curstate.effects;
    key.model_flags = (uint16_t)entity->model->flags;

    int index = entity->index;
    GL3_ASSERT(index >= 0 && index < MaxClientEntities);

    LightingCache &cache = s_lightingCache[index];
    if (!cache.dirty)
    {
        if (!memcmp(&cache.key, &key, sizeof(LightingKey)))
        {
            out_direction = cache.direction;
            return cache.data;
        }
    }
    else
    {
        cache.dirty = false;
    }

    cache.key = key;
    ComputeLightingForKey(cache.direction, cache.data, cache.key);
    out_direction = cache.direction;

    return cache.data;
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
    const LightingData &data = GetCachedLightingData(direction, entity);

    Vector3 color;

    if (data.type == LightingSky)
    {
        color = data.lighting.sky.color;
    }
    else
    {
        color = lightstyleApply(data.lighting.normal.sample);

        Vector3 sample2 = lightstyleApply(data.lighting.normal.intensity[0]);
        float f1 = (sample2.x + sample2.y + sample2.z) / 768.0f;

        sample2 = lightstyleApply(data.lighting.normal.intensity[1]);
        float f2 = (sample2.x + sample2.y + sample2.z) / 768.0f;

        sample2 = lightstyleApply(data.lighting.normal.intensity[2]);
        float f3 = (sample2.x + sample2.y + sample2.z) / 768.0f;

        sample2 = lightstyleApply(data.lighting.normal.intensity[3]);
        float f4 = (sample2.x + sample2.y + sample2.z) / 768.0f;

        direction.x = f4 - f2 - f3 + f1;
        direction.y = f2 - f3 - f4 + f1;
        VectorNormalize(direction);
    }

    if (entity->curstate.renderfx == kRenderFxLightMultiplier && entity->curstate.iuser4 != 10)
    {
        color *= ((float)entity->curstate.iuser4 / 10.0f);
    }

    entity->cvFloorColor = { (unsigned)color.x, (unsigned)color.y, (unsigned)color.z, 0 };

    float scale = Q_max(Q_max(color.x, color.y), color.z);
    if (!scale)
    {
        scale = 1;
    }

    direction *= scale;

    float clientTime = g_engfuncs.GetClientTime();

    for (int i = 0; i < MAX_DLIGHTS; i++)
    {
        dlight_t *dlight = &g_dlights[i];
        if (dlight->die < clientTime)
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
