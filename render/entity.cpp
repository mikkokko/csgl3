#include "stdafx.h"
#include "entity.h"
#include "random.h"
#include "sprite.h"
#include "brush.h"
#include "internal.h"
#include "studio_render.h"
#include "studio_proxy.h"

extern "C" int HUD_AddEntity(int, cl_entity_t *, const char *);

namespace Render
{

constexpr int BucketSize = 512;

enum BucketIndex
{
    BucketBrushSolid,
    BucketBrushAlphaTest,
    BucketStudioSolid,
    BucketSpriteSolid,
    BucketTranslucent,
    BucketBeam,
    BucketCount
};

struct Bucket
{
    int count;
    cl_entity_t *entities[BucketSize];
};

static Bucket s_buckets[BucketCount];

static void AddToBucket(BucketIndex index, cl_entity_t *entity)
{
    Bucket &bucket = s_buckets[index];
    if (bucket.count < BucketSize)
    {
        bucket.entities[bucket.count++] = entity;
    }
}

static float EntityDistanceSquared(cl_entity_t *entity, const Vector3 &point)
{
    model_t *model = entity->model;
    GL3_ASSERT(model); // should never get this fucked

    Vector3 temp = (model->mins + model->maxs) * 0.5f;
    temp = point - (temp + entity->origin);
    return Dot(temp, temp);
}

static bool DistanceCompare(const cl_entity_t *const entity1, const cl_entity_t *const entity2)
{
    return entity1->syncbase > entity2->syncbase;
}

void entitySortTranslucents(const Vector3 &cameraPosition)
{
    // sort translucent entities the same way the engine does
    Bucket &bucket = s_buckets[BucketTranslucent];

    // fucked: store entity distance in syncbase
    for (int i = 0; i < bucket.count; i++)
    {
        cl_entity_t *entity = bucket.entities[i];
        GL3_ASSERT(entity->syncbase == 0.0f);
        entity->syncbase = EntityDistanceSquared(entity, cameraPosition);
    }

    std::sort(bucket.entities, bucket.entities + bucket.count, DistanceCompare);

#ifdef SCHIZO_DEBUG
    for (int i = 0; i < bucket.count; i++)
    {
        cl_entity_t *entity = bucket.entities[i];
        entity->syncbase = 0.0f;
    }
#endif
}

void entityDrawViewmodel(int drawFlags)
{
    // FIXME: r_drawviewmodel and others...
    cl_entity_t *viewmodel = g_engfuncs.GetViewModel();

    // hl1 actually supports other model types for viewmodel but why would you want that
    if (viewmodel->model)
    {
        //GL3_ASSERT(viewmodel->model->type == mod_studio);
    }

    if (viewmodel->model && viewmodel->model->type == mod_studio)
    {
        // FIXME: no need to call these when not drawing??? shouldn't affect gl state at all
        studioBeginModels(true);

        // FIXME: missing stuff (colormap for example...)
        viewmodel->curstate.frame = 0;
        viewmodel->curstate.framerate = 1;

        internalUpdateViewmodelAnimation(viewmodel);

        studioProxyDrawEntity(drawFlags, viewmodel, 1);

        studioEndModels();
    }
}

int entityUpdateRenderAmt(cl_entity_t *entity, const Vector3 &origin, const Vector3 &forward)
{
    int result;
    float dist;

    float time = g_engfuncs.GetClientTime();
    float shift = static_cast<float>(entity->curstate.number) * 363;

    switch (entity->curstate.renderfx)
    {
    case kRenderFxPulseSlow:
        result = static_cast<int>(entity->curstate.renderamt + sinf(shift + 2.0f * time) * 16.0f);
        break;

    case kRenderFxPulseFast:
        result = static_cast<int>(entity->curstate.renderamt + sinf(shift + 8.0f * time) * 16.0f);
        break;

    case kRenderFxPulseSlowWide:
        result = static_cast<int>(entity->curstate.renderamt + sinf(shift + 2.0f * time) * 64.0f);
        break;

    case kRenderFxPulseFastWide:
        result = static_cast<int>(entity->curstate.renderamt + sinf(shift + 8.0f * time) * 64.0f);
        break;

    case kRenderFxFadeSlow:
        if (entity->curstate.renderamt > 0)
        {
            entity->curstate.renderamt -= 1;
        }
        else
        {
            entity->curstate.renderamt = 0;
        }
        result = entity->curstate.renderamt;
        break;

    case kRenderFxFadeFast:
        if (entity->curstate.renderamt > 3)
        {
            entity->curstate.renderamt -= 4;
        }
        else
        {
            entity->curstate.renderamt = 0;
        }
        result = entity->curstate.renderamt;
        break;

    case kRenderFxSolidSlow:
        if (entity->curstate.renderamt < 255)
        {
            entity->curstate.renderamt += 1;
        }
        else
        {
            entity->curstate.renderamt = 255;
        }
        result = entity->curstate.renderamt;
        break;

    case kRenderFxSolidFast:
        if (entity->curstate.renderamt < 255 - 3)
        {
            entity->curstate.renderamt += 4;
        }
        else
        {
            entity->curstate.renderamt = 255;
        }
        result = entity->curstate.renderamt;
        break;

    case kRenderFxStrobeSlow:
        if (sinf(shift + 4.0f * time) * 20.0f <= -1.0f)
        {
            return 0;
        }
        result = entity->curstate.renderamt;
        break;

    case kRenderFxStrobeFast:
        if (sinf(shift + 16.0f * time) * 20.0f <= -1.0f)
        {
            return 0;
        }
        result = entity->curstate.renderamt;
        break;

    case kRenderFxStrobeFaster:
        if (sinf(shift + 36.0f * time) * 20.0f <= -1.0f)
        {
            return 0;
        }
        result = entity->curstate.renderamt;
        break;

    case kRenderFxFlickerSlow:
        if ((sinf(shift + time * 17.0f) + sinf(time * 2.0f)) * 20.0f <= -1.0f)
        {
            return 0;
        }
        result = entity->curstate.renderamt;
        break;

    case kRenderFxFlickerFast:
        if ((sinf(shift + time * 23.0f) + sinf(time * 16.0f)) * 20.0f <= -1.0f)
        {
            return 0;
        }
        result = entity->curstate.renderamt;
        break;

    case kRenderFxDistort:
    case kRenderFxHologram:
        dist = (entity->curstate.renderfx == kRenderFxDistort) ? 1.0f : Dot(entity->origin - origin, forward);
        if (dist <= 0.0f)
        {
            return 0;
        }

        entity->curstate.renderamt = 180;

        if (dist > 100.0f)
        {
            result = static_cast<int>((1.0f - (dist - 100.0f) / 400.0f) * 180.0f);
        }
        else
        {
            result = 180;
        }

        result += randomInt(-32, 31);
        break;

    default:
        result = entity->curstate.renderamt;
        break;
    }

    return Q_clamp(result, 0, 255);
}

// FIXME: clean up this mess...
// the idea here is to reduce state changes, basically if we're going to draw
// a lot of identical sprites, it's not wise to setup, draw, and tear down the
// state for each one of them since we can easily draw them all at once

struct EntityHandlers
{
    void (*begin)();
    void (*draw)(cl_entity_t *entity, float blend);
    void (*end)();
};

static const EntityHandlers s_handlers[] =
{
    // mod_brush
    {
        []() {},
        brushDrawTranslucent,
        brushEndTranslucents
    },

    // mod_sprite
    {
        []()  { spriteBegin(true); },
        [](cl_entity_t *entity, float blend)
        { spriteDraw(entity, blend); },
        []() { spriteEnd(); },
    },

    // mod_alias
    {
        []() {},
        [](cl_entity_t *, float) {}, []() {}
    },

    // mod_studio
    {
        []() { studioBeginModels(false); },
        [](cl_entity_t *entity, float blend) { studioProxyDrawEntity(STUDIO_RENDER | STUDIO_EVENTS, entity, blend); },
        studioEndModels
    }
};

void entityDrawTranslucentEntities(const Vector3 &viewOrigin, const Vector3 &viewForward)
{
    const EntityHandlers *handler = nullptr;

    Bucket &bucket = s_buckets[BucketTranslucent];

    for (int i = 0; i < bucket.count; i++)
    {
        cl_entity_t *entity = bucket.entities[i];

        int renderamt = entityUpdateRenderAmt(entity, viewOrigin, viewForward);
        if (!renderamt)
        {
            // not visible
            continue;
        }

        float blend = static_cast<float>(renderamt) * (1.0f / 255);

        const EntityHandlers *newHandler = &s_handlers[entity->model->type];
        if (handler != newHandler)
        {
            if (handler)
            {
                handler->end();
            }

            handler = newHandler;
            handler->begin();
        }

        // fog update
        int rendermode = entity->curstate.rendermode;
        renderFogEnable(rendermode != kRenderGlow && rendermode != kRenderTransAdd);

        handler->draw(entity, blend);
    }

    if (handler)
    {
        handler->end();
    }
}

void entityDrawSolidBrushes()
{
    Bucket &solid = s_buckets[BucketBrushSolid];
    Bucket &alpha = s_buckets[BucketBrushAlphaTest];
    brushDrawSolids(solid.entities, solid.count, alpha.entities, alpha.count);
}

void entityDrawSolidEntities()
{
    const Bucket &sprites = s_buckets[BucketSpriteSolid];
    if (sprites.count)
    {
        spriteBegin(false);

        for (int i = 0; i < sprites.count; i++)
        {
            // setting renderamt to 1 matches the engine
            spriteDraw(sprites.entities[i], 1.0f);
        }

        spriteEnd();
    }

    // solid studio models might still enable blending thanks to czero, so draw them last
    const Bucket &studios = s_buckets[BucketStudioSolid];
    if (studios.count)
    {
        studioBeginModels(false);

        for (int i = 0; i < studios.count; i++)
        {
            studioProxyDrawEntity(STUDIO_RENDER | STUDIO_EVENTS, studios.entities[i], 1.0f);
        }

        studioEndModels();
    }
}

// returns 0 if the engine should render this instead of us
int AddEntity(int type, struct cl_entity_s *entity)
{
    if (!g_state.active)
    {
        return 0;
    }

    if (type == ET_BEAM)
    {
        AddToBucket(BucketBeam, entity);
        return 1;
    }

    const model_t *model = entity->model;
    if (!model)
    {
        GL3_ASSERT(false);
        return 1;
    }

    // FIXME: always true? should we already compute renderfx here?
    if (entity->curstate.rendermode != kRenderNormal && entity->curstate.renderamt == 0)
    {
        // fully transparent entity, these are actually quite common
        return 1;
    }

    BucketIndex bucketIndex;

    // check if it's translucent
    // all sprites are considered translucent (unless gl_spriteblend is 0)
    if (entity->curstate.rendermode != kRenderNormal || (model->type == mod_sprite && gl_spriteblend->value))
    {
        // alpha tested bruh models are not translucent so they're have they're own bucket
        if (model->type == mod_brush && entity->curstate.rendermode == kRenderTransAlpha)
        {
            bucketIndex = BucketBrushAlphaTest;
        }
        else
        {
            // i guess
            bucketIndex = BucketTranslucent;
        }
    }
    else
    {
        // it's solid
        switch (model->type)
        {
        case mod_brush:
            bucketIndex = BucketBrushSolid;
            break;

        case mod_studio:
            bucketIndex = BucketStudioSolid;
            break;

        case mod_sprite:
            bucketIndex = BucketSpriteSolid;
            break;

        default:
            GL3_ASSERT(false);
            return 1;
        }
    }

    AddToBucket(bucketIndex, entity);
    return 1;
}

static int AddVisibleTempEntity(cl_entity_t *entity)
{
    entity->curstate.angles = entity->angles;
    entity->latched.prevangles = entity->angles;

    // let AddEntity do the pvs culling
    HUD_AddEntity(ET_TEMPENTITY, entity, entity->model->name);
    return 1;
}

AddEntityCallback GetAddEntityCallback(AddEntityCallback original)
{
    if (!g_state.active)
    {
        return original;
    }

    return AddVisibleTempEntity;
}

void entityClearBuckets()
{
    for (Bucket &bucket : s_buckets)
    {
        bucket.count = 0;
    }
}

cl_entity_t **entityGetBeams(int &count)
{
    Bucket &bucket = s_buckets[BucketBeam];
    count = bucket.count;
    return bucket.entities;
}

}
