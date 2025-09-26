#include "stdafx.h"
#include "particle.h"
#include "immediate.h"
#include "random.h"
#include "internal.h"
#include "hudgl3.h"
#include "texture.h"

namespace Render
{

constexpr auto MaxParticles = 2048;

static particle_t s_particles[MaxParticles];

// need to use these shitty lists to stay compatible
static particle_t *s_freeParticles;
static particle_t *s_activeParticles;
static particle_t *s_activeTracers;

static GLuint s_particleTexture;

static GLuint s_tracerTexture;

static cvar_t *s_tracerred;
static cvar_t *s_tracergreen;
static cvar_t *s_tracerblue;
static cvar_t *s_traceralpha;

// easier to just load this as rbga8888
static const uint32_t s_particleTextureData[4][4] = {
    { 0x00FFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00FFFFFF },
    { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { 0x00FFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00FFFFFF },
};

static const int s_ramp1[] = { 111, 109, 107, 105, 103, 101, 99, 97 };
static const int s_ramp2[] = { 111, 110, 109, 108, 107, 106, 104, 102 };
static const int s_ramp3[] = { 109, 107, 6, 5, 4, 3, 0, 0 };
static const int s_sparkRamp[] = { 254, 253, 252, 111, 110, 109, 108, 103, 96 };

static color24 s_tracerColors[] = {
    { 255, 255, 255 },
    { 255, 0, 0 },
    { 0, 255, 0 },
    { 0, 0, 255 },
    { 0, 0, 0 }, // gets set dynamically
    { 255, 167, 17 },
    { 255, 130, 90 },
    { 55, 60, 144 },
    { 255, 130, 90 },
    { 255, 140, 90 },
    { 200, 130, 90 },
    { 255, 120, 70 }
};

static const float s_tracerSizes[] = { 1.5f, 0.5f, 1, 1, 1, 1, 1, 1, 1, 1 };

// palette.lmp, can't be bothered to read it from disk sorry
static const unsigned char s_paletteLump[768] = {
    0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x1F, 0x1F, 0x1F, 0x2F, 0x2F, 0x2F,
    0x3F, 0x3F, 0x3F, 0x4B, 0x4B, 0x4B, 0x5B, 0x5B, 0x5B, 0x6B, 0x6B, 0x6B,
    0x7B, 0x7B, 0x7B, 0x8B, 0x8B, 0x8B, 0x9B, 0x9B, 0x9B, 0xAB, 0xAB, 0xAB,
    0xBB, 0xBB, 0xBB, 0xCB, 0xCB, 0xCB, 0xDB, 0xDB, 0xDB, 0xEB, 0xEB, 0xEB,
    0x0F, 0x0B, 0x07, 0x17, 0x0F, 0x0B, 0x1F, 0x17, 0x0B, 0x27, 0x1B, 0x0F,
    0x2F, 0x23, 0x13, 0x37, 0x2B, 0x17, 0x3F, 0x2F, 0x17, 0x4B, 0x37, 0x1B,
    0x53, 0x3B, 0x1B, 0x5B, 0x43, 0x1F, 0x63, 0x4B, 0x1F, 0x6B, 0x53, 0x1F,
    0x73, 0x57, 0x1F, 0x7B, 0x5F, 0x23, 0x83, 0x67, 0x23, 0x8F, 0x6F, 0x23,
    0x0B, 0x0B, 0x0F, 0x13, 0x13, 0x1B, 0x1B, 0x1B, 0x27, 0x27, 0x27, 0x33,
    0x2F, 0x2F, 0x3F, 0x37, 0x37, 0x4B, 0x3F, 0x3F, 0x57, 0x47, 0x47, 0x67,
    0x4F, 0x4F, 0x73, 0x5B, 0x5B, 0x7F, 0x63, 0x63, 0x8B, 0x6B, 0x6B, 0x97,
    0x73, 0x73, 0xA3, 0x7B, 0x7B, 0xAF, 0x83, 0x83, 0xBB, 0x8B, 0x8B, 0xCB,
    0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x0B, 0x0B, 0x00, 0x13, 0x13, 0x00,
    0x1B, 0x1B, 0x00, 0x23, 0x23, 0x00, 0x2B, 0x2B, 0x07, 0x2F, 0x2F, 0x07,
    0x37, 0x37, 0x07, 0x3F, 0x3F, 0x07, 0x47, 0x47, 0x07, 0x4B, 0x4B, 0x0B,
    0x53, 0x53, 0x0B, 0x5B, 0x5B, 0x0B, 0x63, 0x63, 0x0B, 0x6B, 0x6B, 0x0F,
    0x07, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x17, 0x00, 0x00, 0x1F, 0x00, 0x00,
    0x27, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x37, 0x00, 0x00, 0x3F, 0x00, 0x00,
    0x47, 0x00, 0x00, 0x4F, 0x00, 0x00, 0x57, 0x00, 0x00, 0x5F, 0x00, 0x00,
    0x67, 0x00, 0x00, 0x6F, 0x00, 0x00, 0x77, 0x00, 0x00, 0x7F, 0x00, 0x00,
    0x13, 0x13, 0x00, 0x1B, 0x1B, 0x00, 0x23, 0x23, 0x00, 0x2F, 0x2B, 0x00,
    0x37, 0x2F, 0x00, 0x43, 0x37, 0x00, 0x4B, 0x3B, 0x07, 0x57, 0x43, 0x07,
    0x5F, 0x47, 0x07, 0x6B, 0x4B, 0x0B, 0x77, 0x53, 0x0F, 0x83, 0x57, 0x13,
    0x8B, 0x5B, 0x13, 0x97, 0x5F, 0x1B, 0xA3, 0x63, 0x1F, 0xAF, 0x67, 0x23,
    0x23, 0x13, 0x07, 0x2F, 0x17, 0x0B, 0x3B, 0x1F, 0x0F, 0x4B, 0x23, 0x13,
    0x57, 0x2B, 0x17, 0x63, 0x2F, 0x1F, 0x73, 0x37, 0x23, 0x7F, 0x3B, 0x2B,
    0x8F, 0x43, 0x33, 0x9F, 0x4F, 0x33, 0xAF, 0x63, 0x2F, 0xBF, 0x77, 0x2F,
    0xCF, 0x8F, 0x2B, 0xDF, 0xAB, 0x27, 0xEF, 0xCB, 0x1F, 0xFF, 0xF3, 0x1B,
    0x0B, 0x07, 0x00, 0x1B, 0x13, 0x00, 0x2B, 0x23, 0x0F, 0x37, 0x2B, 0x13,
    0x47, 0x33, 0x1B, 0x53, 0x37, 0x23, 0x63, 0x3F, 0x2B, 0x6F, 0x47, 0x33,
    0x7F, 0x53, 0x3F, 0x8B, 0x5F, 0x47, 0x9B, 0x6B, 0x53, 0xA7, 0x7B, 0x5F,
    0xB7, 0x87, 0x6B, 0xC3, 0x93, 0x7B, 0xD3, 0xA3, 0x8B, 0xE3, 0xB3, 0x97,
    0xAB, 0x8B, 0xA3, 0x9F, 0x7F, 0x97, 0x93, 0x73, 0x87, 0x8B, 0x67, 0x7B,
    0x7F, 0x5B, 0x6F, 0x77, 0x53, 0x63, 0x6B, 0x4B, 0x57, 0x5F, 0x3F, 0x4B,
    0x57, 0x37, 0x43, 0x4B, 0x2F, 0x37, 0x43, 0x27, 0x2F, 0x37, 0x1F, 0x23,
    0x2B, 0x17, 0x1B, 0x23, 0x13, 0x13, 0x17, 0x0B, 0x0B, 0x0F, 0x07, 0x07,
    0xBB, 0x73, 0x9F, 0xAF, 0x6B, 0x8F, 0xA3, 0x5F, 0x83, 0x97, 0x57, 0x77,
    0x8B, 0x4F, 0x6B, 0x7F, 0x4B, 0x5F, 0x73, 0x43, 0x53, 0x6B, 0x3B, 0x4B,
    0x5F, 0x33, 0x3F, 0x53, 0x2B, 0x37, 0x47, 0x23, 0x2B, 0x3B, 0x1F, 0x23,
    0x2F, 0x17, 0x1B, 0x23, 0x13, 0x13, 0x17, 0x0B, 0x0B, 0x0F, 0x07, 0x07,
    0xDB, 0xC3, 0xBB, 0xCB, 0xB3, 0xA7, 0xBF, 0xA3, 0x9B, 0xAF, 0x97, 0x8B,
    0xA3, 0x87, 0x7B, 0x97, 0x7B, 0x6F, 0x87, 0x6F, 0x5F, 0x7B, 0x63, 0x53,
    0x6B, 0x57, 0x47, 0x5F, 0x4B, 0x3B, 0x53, 0x3F, 0x33, 0x43, 0x33, 0x27,
    0x37, 0x2B, 0x1F, 0x27, 0x1F, 0x17, 0x1B, 0x13, 0x0F, 0x0F, 0x0B, 0x07,
    0x6F, 0x83, 0x7B, 0x67, 0x7B, 0x6F, 0x5F, 0x73, 0x67, 0x57, 0x6B, 0x5F,
    0x4F, 0x63, 0x57, 0x47, 0x5B, 0x4F, 0x3F, 0x53, 0x47, 0x37, 0x4B, 0x3F,
    0x2F, 0x43, 0x37, 0x2B, 0x3B, 0x2F, 0x23, 0x33, 0x27, 0x1F, 0x2B, 0x1F,
    0x17, 0x23, 0x17, 0x0F, 0x1B, 0x13, 0x0B, 0x13, 0x0B, 0x07, 0x0B, 0x07,
    0xFF, 0xF3, 0x1B, 0xEF, 0xDF, 0x17, 0xDB, 0xCB, 0x13, 0xCB, 0xB7, 0x0F,
    0xBB, 0xA7, 0x0F, 0xAB, 0x97, 0x0B, 0x9B, 0x83, 0x07, 0x8B, 0x73, 0x07,
    0x7B, 0x63, 0x07, 0x6B, 0x53, 0x00, 0x5B, 0x47, 0x00, 0x4B, 0x37, 0x00,
    0x3B, 0x2B, 0x00, 0x2B, 0x1F, 0x00, 0x1B, 0x0F, 0x00, 0x0B, 0x07, 0x00,
    0x00, 0x00, 0xFF, 0x0B, 0x0B, 0xEF, 0x13, 0x13, 0xDF, 0x1B, 0x1B, 0xCF,
    0x23, 0x23, 0xBF, 0x2B, 0x2B, 0xAF, 0x2F, 0x2F, 0x9F, 0x2F, 0x2F, 0x8F,
    0x2F, 0x2F, 0x7F, 0x2F, 0x2F, 0x6F, 0x2F, 0x2F, 0x5F, 0x2B, 0x2B, 0x4F,
    0x23, 0x23, 0x3F, 0x1B, 0x1B, 0x2F, 0x13, 0x13, 0x1F, 0x0B, 0x0B, 0x0F,
    0x2B, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x4B, 0x07, 0x00, 0x5F, 0x07, 0x00,
    0x6F, 0x0F, 0x00, 0x7F, 0x17, 0x07, 0x93, 0x1F, 0x07, 0xA3, 0x27, 0x0B,
    0xB7, 0x33, 0x0F, 0xC3, 0x4B, 0x1B, 0xCF, 0x63, 0x2B, 0xDB, 0x7F, 0x3B,
    0xE3, 0x97, 0x4F, 0xE7, 0xAB, 0x5F, 0xEF, 0xBF, 0x77, 0xF7, 0xD3, 0x8B,
    0xA7, 0x7B, 0x3B, 0xB7, 0x9B, 0x37, 0xC7, 0xC3, 0x37, 0xE7, 0xE3, 0x57,
    0x00, 0xFF, 0x00, 0xAB, 0xE7, 0xFF, 0xD7, 0xFF, 0xFF, 0x67, 0x00, 0x00,
    0x8B, 0x00, 0x00, 0xB3, 0x00, 0x00, 0xD7, 0x00, 0x00, 0xFF, 0x00, 0x00,
    0xFF, 0xF3, 0x93, 0xFF, 0xF7, 0xC7, 0xFF, 0xFF, 0xFF, 0x9F, 0x5B, 0x53
};

inline void GetPaletteColor(int index, byte (&rgb)[3])
{
    GL3_ASSERT(index >= 0 && index < 256);
    const byte *data = &s_paletteLump[index * 3];
    rgb[0] = data[0];
    rgb[1] = data[1];
    rgb[2] = data[2];
}

void particleInit()
{
    // always going to have linear filtering
    {
        textureGenTextures(1, &s_particleTexture);
        glBindTexture(GL_TEXTURE_2D, s_particleTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, s_particleTextureData);
    }

    // this is probably not going to change so get it here
    model_t *dotSprite = g_engineStudio.Mod_ForName("sprites/dot.spr", true);
    if (dotSprite)
    {
        SpriteInfo spriteInfo;
        if (internalGetSpriteInfo(dotSprite, 0, &spriteInfo))
        {
            s_tracerTexture = spriteInfo.texture;
        }
    }

    s_tracerred = g_engfuncs.pfnGetCvarPointer("tracerred");
    s_tracergreen = g_engfuncs.pfnGetCvarPointer("tracergreen");
    s_tracerblue = g_engfuncs.pfnGetCvarPointer("tracerblue");
    s_traceralpha = g_engfuncs.pfnGetCvarPointer("traceralpha");

    particleClear();
}

void particleClear()
{
    s_activeParticles = nullptr;
    s_activeTracers = nullptr;

    s_freeParticles = &s_particles[0];

    for (int i = 0; i < MaxParticles; i++)
    {
        s_particles[i].next = &s_particles[i + 1];
    }

    s_particles[MaxParticles - 1].next = nullptr;
}

static particle_t *AllocateParticle(particle_t **head)
{
    particle_t *particle = s_freeParticles;
    if (!particle)
    {
        return nullptr;
    }

    s_freeParticles = s_freeParticles->next;

    particle->next = *head;
    *head = particle;

    return particle;
}

// got confused by valve's shitty code so this was rewritten, in practice should work the same
static void FreeDeadParticles(particle_t **head)
{
    float time = g_engfuncs.GetClientTime();

    particle_t **current = head;
    while (*current)
    {
        particle_t *particle = *current;
        if (particle->die < time)
        {
            if (particle->deathfunc)
            {
                particle->deathfunc(particle);
                particle->deathfunc = nullptr;
            }

            *current = particle->next;

            particle->next = s_freeParticles;
            s_freeParticles = particle;
        }
        else
        {
            current = &particle->next;
        }
    }
}

particle_t *particleAllocate()
{
    return AllocateParticle(&s_activeParticles);
}

particle_t *particleAllocateTracer()
{
    return AllocateParticle(&s_activeTracers);
}

static void DrawTracer(particle_t *tracer, float camSide)
{
    Vector3 point1 = tracer->org;

    float speed = Q_min(tracer->die - 0.1f, 0.1f) * tracer->ramp;
    Vector3 point2 = tracer->org + tracer->vel * speed;

    Vector2 screen1, screen2;
    bool behind1 = hudWorldToScreen(point1, screen1.x, screen1.y);
    bool behind2 = hudWorldToScreen(point2, screen2.x, screen2.y);

    if (behind1 || behind2)
    {
        float d1 = Dot(point1, g_state.viewForward) - camSide;
        float d2 = Dot(point2, g_state.viewForward) - camSide;
        if (d1 <= 0 && d2 <= 0)
        {
            return;
        }

        float denom = d2 - d1;
        if (denom < 0.01f)
        {
            return;
        }

        float fraction = d1 / denom;
        if (d1 > 0)
        {
            point2 = VectorLerp(point1, point2, fraction);
        }
        else
        {
            point1 = VectorLerp(point1, point2, fraction);
        }

        hudWorldToScreen(point1, screen1.x, screen1.y);
        hudWorldToScreen(point2, screen2.x, screen2.y);
    }

    Vector3 dir{ screen2.x - screen1.x, screen2.y - screen1.y, 0.0f };
    VectorNormalize(dir);

    float size = s_tracerSizes[tracer->type];
    Vector3 offset = g_state.viewUp * (dir.x * size) - g_state.viewRight * (dir.y * size);

    immediateBegin(GL_QUADS);

    color24 color = s_tracerColors[tracer->color];
    float alpha = static_cast<uint8_t>(tracer->packedColor) * (1.0f / 255);
    float red = color.r * (1.0f / 255) * alpha;
    float green = color.g * (1.0f / 255) * alpha;
    float blue = color.b * (1.0f / 255) * alpha;

    immediateColor4f(0, 0, 0, 1);
    immediateTexCoord2f(0, 0);
    Vector3 v1 = point1 + offset;
    immediateVertex3f(v1.x, v1.y, v1.z);

    immediateColor4f(red, green, blue, 1);
    immediateTexCoord2f(0, 1);
    Vector3 v2 = point2 + offset;
    immediateVertex3f(v2.x, v2.y, v2.z);

    immediateColor4f(red, green, blue, 1);
    immediateTexCoord2f(1, 1);
    Vector3 v3 = point2 - offset;
    immediateVertex3f(v3.x, v3.y, v3.z);

    immediateColor4f(0, 0, 0, 1);
    immediateTexCoord2f(1, 0);
    Vector3 v4 = point1 - offset;
    immediateVertex3f(v4.x, v4.y, v4.z);

    immediateEnd();
}

static void UpdateTracer(particle_t *tracer, float frametime, float gravity, float accel)
{
    tracer->org += tracer->vel * frametime;

    if (tracer->type == pt_grav)
    {
        tracer->vel.z -= gravity;
        tracer->vel.x *= accel;
        tracer->vel.y *= accel;

        float color = ((tracer->die - g_engfuncs.GetClientTime()) * 2.0f) * 255.0f;
        tracer->packedColor = static_cast<short>(color);
        if (tracer->packedColor > 255)
        {
            tracer->packedColor = 255;
        }
    }
    else if (tracer->type == pt_slowgrav)
    {
        tracer->vel.z = gravity * 0.05f;
    }
}

static void DrawTracers()
{
    FreeDeadParticles(&s_activeTracers);
    if (!s_activeTracers)
    {
        return;
    }

    if (!s_tracerTexture)
    {
        GL3_ASSERT(false);
        return; // matches goldsrc behaviour
    }

    float alpha = s_traceralpha->value;
    s_tracerColors[4].r = static_cast<uint8_t>(s_tracerred->value * alpha * 255.0f);
    s_tracerColors[4].g = static_cast<uint8_t>(s_tracergreen->value * alpha * 255.0f);
    s_tracerColors[4].b = static_cast<uint8_t>(s_tracerblue->value * alpha * 255.0f);

    immediateDrawStart(false);

    immediateBindTexture(s_tracerTexture);

    // FIXME: won't work with older builds (hudGetClientOldTime doesn't exist)
    float frametime = g_engfuncs.GetClientTime() - g_engfuncs.hudGetClientOldTime();
    float accel = Q_max(1.0f - frametime * 0.9f, 0.0f);

    float gravity = g_state.movevars->gravity * frametime;
    float camSide = Dot(g_state.viewOrigin, g_state.viewForward);

    immediateBlendEnable(GL_TRUE);
    immediateBlendFunc(GL_ONE, GL_ONE);
    immediateDepthMask(GL_FALSE);

    immediateCullFace(GL_FALSE);

    for (particle_t *tracer = s_activeTracers; tracer; tracer = tracer->next)
    {
        DrawTracer(tracer, camSide);
        UpdateTracer(tracer, frametime, gravity, accel);
    }

    immediateCullFace(GL_TRUE);

    immediateBlendEnable(GL_FALSE);
    immediateDepthMask(GL_TRUE);

    immediateDrawEnd();
}

static void ParticleDraw(particle_t *particle, const Vector3 &right, const Vector3 &up)
{
    float scale = Dot(particle->org - g_state.viewOrigin, g_state.viewForward);
    if (scale < 20.0f)
    {
        scale = 1.0f;
    }
    else
    {
        scale = 1.0f + scale * 0.004f;
    }

    scale *= 0.5f;

    byte color[3];
    GetPaletteColor(particle->color, color);
    immediateColor4f(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, 1.0f);

    immediateTexCoord2f(0, 0);
    Vector3 v1 = particle->org;
    immediateVertex3f(v1.x, v1.y, v1.z);

    immediateTexCoord2f(1, 0);
    Vector3 v2 = particle->org + up * scale;
    immediateVertex3f(v2.x, v2.y, v2.z);

    immediateTexCoord2f(1, 1);
    Vector3 v3 = particle->org + up * scale + right * scale;
    immediateVertex3f(v3.x, v3.y, v3.z);

    immediateTexCoord2f(0, 1);
    Vector3 v4 = particle->org + right * scale;
    immediateVertex3f(v4.x, v4.y, v4.z);
}

static void ParticleUpdate(particle_t *particle, float frametime, float gravity)
{
    if (particle->type == pt_clientcustom)
    {
        if (particle->callback)
        {
            particle->callback(particle, frametime);
        }

        return;
    }

    particle->org += particle->vel * frametime;

    switch (particle->type)
    {
    case pt_grav:
        particle->vel.z -= gravity * 20;
        break;

    case pt_slowgrav:
        particle->vel.z = gravity;
        break;

    case pt_fire:
        particle->ramp += frametime * 5;
        if (particle->ramp < 6)
        {
            particle->packedColor = 0;
            particle->color = (short)s_ramp3[(int)particle->ramp];
        }
        else
        {
            particle->die = -1;
        }
        particle->vel.z += gravity;
        break;

    case pt_explode:
        particle->ramp += frametime * 10;
        if (particle->ramp < 8)
        {
            particle->packedColor = 0;
            particle->color = (short)s_ramp1[(int)particle->ramp];
        }
        else
        {
            particle->die = -1;
        }
        particle->vel += particle->vel * frametime * 4;
        particle->vel.z -= gravity;
        break;

    case pt_explode2:
        particle->ramp += frametime * 15;
        if (particle->ramp < 8)
        {
            particle->packedColor = 0;
            particle->color = (short)s_ramp2[(int)particle->ramp];
        }
        else
        {
            particle->die = -1;
        }
        particle->vel -= particle->vel * frametime;
        particle->vel.z -= gravity;
        break;

    case pt_blob:
    case pt_blob2:
        particle->ramp += frametime * 10;
        if (particle->ramp >= 9)
        {
            particle->ramp = 0;
        }
        particle->color = (unsigned short)s_sparkRamp[(int)particle->ramp];
        particle->packedColor = 0;

        particle->vel.x -= particle->vel.x * 0.5f * frametime;
        particle->vel.y -= particle->vel.y * 0.5f * frametime;
        particle->vel.z -= gravity * 5;

        particle->type = randomInt(0, 3) ? pt_blob : pt_blob2;
        break;

    case pt_vox_slowgrav:
        particle->vel.z -= gravity * 4;
        break;

    case pt_vox_grav:
        particle->vel.z -= gravity * 8;
        break;

    default:
        GL3_ASSERT(false);
        break;
    }
}

static void DrawParticles()
{
    FreeDeadParticles(&s_activeParticles);
    if (!s_activeParticles)
    {
        return;
    }

    // FIXME: goldsrc does alpha testing as well...?
    // looks like shit though so we're probably not ging to
    immediateDrawStart(false);

    immediateBindTexture(s_particleTexture);

    immediateBlendEnable(GL_TRUE);
    immediateBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    immediateBegin(GL_QUADS);

    Vector3 right = g_state.viewRight * 1.5f;
    Vector3 up = g_state.viewUp * 1.5f;

    // FIXME: won't work with very old engine versions (no hudGetClientOldTime)
    float frametime = g_engfuncs.GetClientTime() - g_engfuncs.hudGetClientOldTime();

    float gravity = g_state.movevars->gravity * 0.05f * frametime;

    for (particle_t *particle = s_activeParticles; particle; particle = particle->next)
    {
        if (particle->type != pt_blob)
        {
            ParticleDraw(particle, right, up);
        }

        ParticleUpdate(particle, frametime, gravity);
    }

    immediateEnd();

    immediateDrawEnd();
}

void particleDraw()
{
    DrawParticles();
    DrawTracers();
}

}
