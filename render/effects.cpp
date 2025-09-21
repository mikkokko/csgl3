#include "stdafx.h"
#include "effects.h"
#include "random.h"
#include "particle.h"

namespace Render
{

// a few functions are passed through
static efx_api_t s_engineEfx;

static model_t *s_muzzleflashSprites[3];

#define NOT_IMPL() g_engfuncs.Con_Printf("[EFX] %s not implemented", __func__)

// fucked up: when nvgs are on, CL_AllocDlight is called *every frame* to allocate a 0.1 seconds lasting dlight
// this means that CL_AllocDlight will run out of dlights, and every call to it will iterate through the
// entire array trying to find a free spot.. this takes a lot of cpu time
// to fix this, try not do do that, actually disable nvg dlights entirely, they look like shit anyway

// populated when CL_AllocDlight is invoked within HUD_Redraw
// if it's an nvg dlight, we'll let the renderer know
// otherwise they're added as normal
static int s_capturedDlightCount;
static dlight_t s_capturedDlights[MAX_DLIGHTS];
static bool s_capturingDlights;

static void R_SparkStreaks(float *pos, int count, int velocityMin, int velocityMax);

void effectsBeginDlightCapture()
{
    s_capturingDlights = true;
}

void effectsEndDlightCapture()
{
    s_capturingDlights = false;

    for (int i = 0; i < s_capturedDlightCount; i++)
    {
        dlight_t &light = s_capturedDlights[i];
        if (light.color.r == 1 && light.color.g == 20 && light.color.b == 1)
        {
            // ignore nvg dlights
        }
        else
        {
            dlight_t *memory = g_engfuncs.pEfxAPI->CL_AllocDlight(light.key);
            *memory = light; // should be fine (key is already set)
        }
    }

    s_capturedDlightCount = 0;
}

// FIXME: not tested
static particle_t *R_AllocParticle(void (*callback)(struct particle_s *particle, float frametime))
{
    if (!g_state.active)
    {
        return s_engineEfx.R_AllocParticle(callback);
    }

    particle_t *particle = particleAllocate();
    if (!particle)
    {
        g_engfuncs.Con_Printf("Not enough free particles\n");
        return nullptr;
    }

    particle->die = g_engfuncs.GetClientTime();
    particle->color = 0;
    particle->callback = callback;
    particle->deathfunc = nullptr;
    particle->ramp = 0;
    particle->org = {};
    particle->vel = {};
    particle->packedColor = 0;

    return particle;
}

// FIXME: not tested
static void R_BlobExplosion(float *org)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BlobExplosion(org);
    }

    for (int i = 0; i < 1024; i++)
    {
        particle_t *particle = particleAllocate();
        if (!particle)
        {
            break;
        }

        particle->die = g_engfuncs.GetClientTime() + randomFloat(1.0f, 1.4f);
        if (i & 1)
        {
            particle->type = pt_blob;
            particle->color = (short)randomInt(66, 71);
        }
        else
        {
            particle->type = pt_blob2;
            particle->color = (short)randomInt(150, 155);
        }

        particle->packedColor = 0;

        particle->org.x = org[0] + randomFloat(-16, 16);
        particle->org.y = org[1] + randomFloat(-16, 16);
        particle->org.z = org[2] + randomFloat(-16, 16);

        particle->vel.x = randomFloat(-256, 256);
        particle->vel.y = randomFloat(-256, 256);
        particle->vel.z = randomFloat(-256, 256);
    }
}

static void R_Blood(float *org, float *dir, int pcolor, int speed)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_Blood(org, dir, pcolor, speed);
    }

    // uses particles
    NOT_IMPL();
}

static void R_BloodStream(float *org, float *dir, int pcolor, int speed)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BloodStream(org, dir, pcolor, speed);
    }

    // uses particles
    NOT_IMPL();
}

static void R_BulletImpactParticles(float *pos)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BulletImpactParticles(pos);
    }

    int count = 4;
    int color = 3;

    float distance = VectorLength(Vector3{ pos } - g_state.viewOrigin);
    if (distance <= 1000.0f)
    {
        int dist = static_cast<int>((1000.0f - distance) / 100.0f);
        if (dist)
        {
            count = 4 * dist;
            color = 3 - 30 * dist / 100;
        }
    }

    R_SparkStreaks(pos, 2, -200, 200);

    for (int i = 0; i < count; i++)
    {
        particle_t *particle = particleAllocate();
        if (particle)
        {
            particle->org = pos;
            particle->vel.x = randomFloat(-1, 1);
            particle->vel.y = randomFloat(-1, 1);
            particle->vel.z = randomFloat(-1, 1);
            particle->vel *= randomFloat(50, 100);
            particle->packedColor = 0;
            particle->type = pt_grav;
            particle->color = (short)(3 - color);
            particle->die = g_engfuncs.GetClientTime() + 0.5f;
        }
    }
}

static void R_EntityParticles(struct cl_entity_s *ent)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_EntityParticles(ent);
    }

    // uses particles
    NOT_IMPL();
}

static void R_FlickerParticles(float *org)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_FlickerParticles(org);
    }

    for (int i = 0; i < 15; i++)
    {
        particle_t *particle = particleAllocate();
        if (!particle)
        {
            break;
        }

        particle->org = org;
        particle->vel.x = randomFloat(-32, 32);
        particle->vel.y = randomFloat(-32, 32);
        particle->vel.z = randomFloat(80, 143);
        particle->die = g_engfuncs.GetClientTime() + 2;
        particle->ramp = 0;
        particle->color = 254;
        particle->packedColor = 0;
        particle->type = pt_blob2;
    }
}

static void R_Implosion(float *end, float radius, int count, float life)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_Implosion(end, radius, count, life);
    }

    // uses particles
    NOT_IMPL();
}

static void R_LargeFunnel(float *org, int reverse)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_LargeFunnel(org, reverse);
    }

    // uses particles
    NOT_IMPL();
}

static void R_LavaSplash(float *org)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_LavaSplash(org);
    }

    // uses particles
    NOT_IMPL();
}

// engine's R_MuzzleFlash calls AppendTEntity directly, which won't work for us
static void R_MuzzleFlash(float *pos1, int type)
{
    if (!g_state.active)
    {
        s_engineEfx.R_MuzzleFlash(pos1, type);
        return;
    }

    int idx = (type % 10) % 3;

    float scale = (float)(type / 10) * 0.1f;
    if (scale == 0.0f)
    {
        scale = 0.5f;
    }

    // this will break if the model is a studio model, but that won't ever happen... right
    int frameMax = s_muzzleflashSprites[idx]->numframes;

    TEMPENTITY *tent = s_engineEfx.CL_TempEntAlloc(pos1, s_muzzleflashSprites[idx]);
    if (tent)
    {
        tent->die = g_engfuncs.GetClientTime() + 0.01f;
        tent->frameMax = (float)frameMax;

        cl_entity_t &entity = tent->entity;
        entity.curstate.rendermode = kRenderTransAdd;
        entity.curstate.renderamt = 255;
        entity.curstate.renderfx = 0;
        entity.curstate.scale = scale;
        entity.curstate.rendercolor.r = 255;
        entity.curstate.rendercolor.g = 255;
        entity.curstate.rendercolor.b = 255;
        entity.origin = pos1;
        entity.angles = {};
        entity.curstate.frame = (float)randomInt(0, frameMax - 1);

        entity.angles.z = (float)randomInt(0, idx ? 359 : 20);

        // FIXME: does this work as intended now???
        AddEntity(ET_TEMPENTITY, &entity);
    }
}

static void R_ParticleBox(float *mins, float *maxs, unsigned char r, unsigned char g, unsigned char b, float life)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_ParticleBox(mins, maxs, r, g, b, life);
    }

    // uses particles
    NOT_IMPL();
}

static void R_ParticleBurst(float *pos, int size, int color, float life)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_ParticleBurst(pos, size, color, life);
    }

    // uses particles
    NOT_IMPL();
}

static void R_ParticleExplosion(float *org)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_ParticleExplosion(org);
    }

    // uses particles
    NOT_IMPL();
}

static void R_ParticleExplosion2(float *org, int colorStart, int colorLength)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_ParticleExplosion2(org, colorStart, colorLength);
    }

    // uses particles
    NOT_IMPL();
}

static void R_ParticleLine(float *start, float *end, unsigned char r, unsigned char g, unsigned char b, float life)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_ParticleLine(start, end, r, g, b, life);
    }

    // uses particles
    NOT_IMPL();
}

static void R_RocketTrail(float *start, float *end, int type)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_RocketTrail(start, end, type);
    }

    // uses particles
    NOT_IMPL();
}

static void R_RunParticleEffect(float *org, float *dir, int color, int count)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_RunParticleEffect(org, dir, color, count);
    }

    // uses particles
    NOT_IMPL();
}

static void R_ShowLine(float *start, float *end)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_ShowLine(start, end);
    }

    // uses particles
    NOT_IMPL();
}

static void R_SparkStreaks(float *pos, int count, int velocityMin, int velocityMax)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_SparkStreaks(pos, count, velocityMin, velocityMax);
    }

    for (int i = 0; i < count; i++)
    {
        particle_t *tracer = particleAllocateTracer();
        if (!tracer)
        {
            break;
        }

        tracer->die = g_engfuncs.GetClientTime() + randomFloat(0.1f, 0.5f);
        tracer->color = 5;
        tracer->packedColor = 255;
        tracer->type = pt_grav;
        tracer->ramp = 0.5f;
        tracer->org = pos;
        tracer->vel.x = randomFloat(static_cast<float>(velocityMin), static_cast<float>(velocityMax));
        tracer->vel.y = randomFloat(static_cast<float>(velocityMin), static_cast<float>(velocityMax));
        tracer->vel.z = randomFloat(static_cast<float>(velocityMin), static_cast<float>(velocityMax));
    }
}

static void R_StreakSplash(float *pos, float *dir, int color, int count, float speed, int velocityMin, int velocityMax)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_StreakSplash(pos, dir, color, count, speed, velocityMin, velocityMax);
    }

    Vector3 velocity = Vector3{ dir } * speed;

    for (int i = 0; i < count; i++)
    {
        particle_t *particle = particleAllocateTracer();
        if (!particle)
        {
            break;
        }

        particle->die = g_engfuncs.GetClientTime() + randomFloat(0.1f, 0.5f);
        particle->color = (short)color;
        particle->packedColor = 255;
        particle->type = pt_grav;
        particle->ramp = 1.0f;
        particle->org = pos;
        particle->vel = velocity;
        particle->vel.x += randomFloat(static_cast<float>(velocityMin), static_cast<float>(velocityMax));
        particle->vel.y += randomFloat(static_cast<float>(velocityMin), static_cast<float>(velocityMax));
        particle->vel.z += randomFloat(static_cast<float>(velocityMin), static_cast<float>(velocityMax));
    }
}

static void R_UserTracerParticle(float *org, float *vel, float life, int colorIndex, float length, unsigned char deathcontext, void (*deathfunc)(struct particle_s *particle))
{
    if (!g_state.active)
    {
        return s_engineEfx.R_UserTracerParticle(org, vel, life, colorIndex, length, deathcontext, deathfunc);
    }

    // uses particles
    NOT_IMPL();
}

static particle_t *R_TracerParticles(float *org, float *vel, float life)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_TracerParticles(org, vel, life);
    }

    // uses particles
    NOT_IMPL();
    return {};
}

static void R_TeleportSplash(float *org)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_TeleportSplash(org);
    }

    // uses particles
    NOT_IMPL();
}

static BEAM *R_BeamCirclePoints(int type, float *start, float *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BeamCirclePoints(type, start, end, modelIndex, life, width, amplitude, brightness, speed, startFrame, framerate, r, g, b);
    }

    NOT_IMPL();
    return {};
}

static BEAM *R_BeamEntPoint(int startEnt, float *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BeamEntPoint(startEnt, end, modelIndex, life, width, amplitude, brightness, speed, startFrame, framerate, r, g, b);
    }

    NOT_IMPL();
    return {};
}

static BEAM *R_BeamEnts(int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BeamEnts(startEnt, endEnt, modelIndex, life, width, amplitude, brightness, speed, startFrame, framerate, r, g, b);
    }

    NOT_IMPL();
    return {};
}

static BEAM *R_BeamFollow(int startEnt, int modelIndex, float life, float width, float r, float g, float b, float brightness)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BeamFollow(startEnt, modelIndex, life, width, r, g, b, brightness);
    }

    NOT_IMPL();
    return {};
}

static void R_BeamKill(int deadEntity)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BeamKill(deadEntity);
    }

    NOT_IMPL();
}

static BEAM *R_BeamLightning(float *start, float *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BeamLightning(start, end, modelIndex, life, width, amplitude, brightness, speed);
    }

    NOT_IMPL();
    return {};
}

static BEAM *R_BeamPoints(float *start, float *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BeamPoints(start, end, modelIndex, life, width, amplitude, brightness, speed, startFrame, framerate, r, g, b);
    }

    NOT_IMPL();
    return {};
}

static BEAM *R_BeamRing(int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b)
{
    if (!g_state.active)
    {
        return s_engineEfx.R_BeamRing(startEnt, endEnt, modelIndex, life, width, amplitude, brightness, speed, startFrame, framerate, r, g, b);
    }

    NOT_IMPL();
    return {};
}

static dlight_t *CL_AllocDlight(int k)
{
    if (s_capturingDlights)
    {
        if (s_capturedDlightCount < Q_countof(s_capturedDlights))
        {
            dlight_t *result = &s_capturedDlights[s_capturedDlightCount++];
            result->key = k;
            return result;
        }
    }

    return s_engineEfx.CL_AllocDlight(k);
}

void effectsInit(efx_api_t *efx_api, engine_studio_api_t *studio)
{
    s_engineEfx = *efx_api;

    // we only need to hook methods that use particles or beams
    efx_api->R_AllocParticle = R_AllocParticle;
    efx_api->R_BlobExplosion = R_BlobExplosion;
    efx_api->R_Blood = R_Blood;
    //efx_api->R_BloodSprite = R_BloodSprite;
    efx_api->R_BloodStream = R_BloodStream;
    //efx_api->R_BreakModel = R_BreakModel;
    //efx_api->R_Bubbles = R_Bubbles;
    //efx_api->R_BubbleTrail = R_BubbleTrail;
    efx_api->R_BulletImpactParticles = R_BulletImpactParticles;
    efx_api->R_EntityParticles = R_EntityParticles;
    //efx_api->R_Explosion = R_Explosion;
    //efx_api->R_FizzEffect = R_FizzEffect;
    //efx_api->R_FireField = R_FireField;
    efx_api->R_FlickerParticles = R_FlickerParticles;
    //efx_api->R_FunnelSprite = R_FunnelSprite;
    efx_api->R_Implosion = R_Implosion;
    efx_api->R_LargeFunnel = R_LargeFunnel;
    efx_api->R_LavaSplash = R_LavaSplash;
    //efx_api->R_MultiGunshot = R_MultiGunshot;
    efx_api->R_MuzzleFlash = R_MuzzleFlash;
    efx_api->R_ParticleBox = R_ParticleBox;
    efx_api->R_ParticleBurst = R_ParticleBurst;
    efx_api->R_ParticleExplosion = R_ParticleExplosion;
    efx_api->R_ParticleExplosion2 = R_ParticleExplosion2;
    efx_api->R_ParticleLine = R_ParticleLine;
    //efx_api->R_PlayerSprites = R_PlayerSprites;
    //efx_api->R_Projectile = R_Projectile;
    //efx_api->R_RicochetSound = R_RicochetSound;
    //efx_api->R_RicochetSprite = R_RicochetSprite;
    //efx_api->R_RocketFlare = R_RocketFlare;
    efx_api->R_RocketTrail = R_RocketTrail;
    efx_api->R_RunParticleEffect = R_RunParticleEffect;
    efx_api->R_ShowLine = R_ShowLine;
    //efx_api->R_SparkEffect = R_SparkEffect;
    //efx_api->R_SparkShower = R_SparkShower;
    efx_api->R_SparkStreaks = R_SparkStreaks;
    //efx_api->R_Spray = R_Spray;
    //efx_api->R_Sprite_Explode = R_Sprite_Explode;
    //efx_api->R_Sprite_Smoke = R_Sprite_Smoke;
    //efx_api->R_Sprite_Spray = R_Sprite_Spray;
    //efx_api->R_Sprite_Trail = R_Sprite_Trail;
    //efx_api->R_Sprite_WallPuff = R_Sprite_WallPuff;
    efx_api->R_StreakSplash = R_StreakSplash;
    //efx_api->R_TracerEffect = R_TracerEffect;
    efx_api->R_UserTracerParticle = R_UserTracerParticle;
    efx_api->R_TracerParticles = R_TracerParticles;
    efx_api->R_TeleportSplash = R_TeleportSplash;
    //efx_api->R_TempSphereModel = R_TempSphereModel;
    //efx_api->R_TempModel = R_TempModel;
    //efx_api->R_DefaultSprite = R_DefaultSprite;
    //efx_api->R_TempSprite = R_TempSprite;
    //efx_api->Draw_DecalIndex = Draw_DecalIndex;
    //efx_api->Draw_DecalIndexFromName = Draw_DecalIndexFromName;
    //efx_api->R_DecalShoot = R_DecalShoot;
    //efx_api->R_AttachTentToPlayer = R_AttachTentToPlayer;
    //efx_api->R_KillAttachedTents = R_KillAttachedTents;
    efx_api->R_BeamCirclePoints = R_BeamCirclePoints;
    efx_api->R_BeamEntPoint = R_BeamEntPoint;
    efx_api->R_BeamEnts = R_BeamEnts;
    efx_api->R_BeamFollow = R_BeamFollow;
    efx_api->R_BeamKill = R_BeamKill;
    efx_api->R_BeamLightning = R_BeamLightning;
    efx_api->R_BeamPoints = R_BeamPoints;
    efx_api->R_BeamRing = R_BeamRing;
    efx_api->CL_AllocDlight = CL_AllocDlight;
    //efx_api->CL_AllocElight = CL_AllocElight;
    //efx_api->CL_TempEntAlloc = CL_TempEntAlloc;
    //efx_api->CL_TempEntAllocNoModel = CL_TempEntAllocNoModel;
    //efx_api->CL_TempEntAllocHigh = CL_TempEntAllocHigh;
    //efx_api->CL_TentEntAllocCustom = CL_TentEntAllocCustom;
    //efx_api->R_GetPackedColor = R_GetPackedColor;
    //efx_api->R_LookupColor = R_LookupColor;
    //efx_api->R_DecalRemoveAll = R_DecalRemoveAll;
    //efx_api->R_FireCustomDecal = R_FireCustomDecal;

    // local tempent slop
    s_muzzleflashSprites[0] = studio->Mod_ForName("sprites/muzzleflash1.spr", true);
    s_muzzleflashSprites[1] = studio->Mod_ForName("sprites/muzzleflash2.spr", true);
    s_muzzleflashSprites[2] = studio->Mod_ForName("sprites/muzzleflash3.spr", true);

    // mark as client models (should add a constant for this...)
    s_muzzleflashSprites[0]->needload = 3;
    s_muzzleflashSprites[1]->needload = 3;
    s_muzzleflashSprites[2]->needload = 3;
}

}
