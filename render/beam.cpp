#include "stdafx.h"
#include "beam.h"
#include "entity.h"
#include "immediate.h"
#include "random.h"

namespace Render
{

static BEAM SetupBeam(
    const Vector3 &start,
    const Vector3 &end,
    int modelIndex,
    float life,
    float width,
    float amplitude,
    float brightness,
    float speed)
{
    BEAM beam{};

    model_t *model = g_engfuncs.hudGetModelByIndex(modelIndex);
    if (model)
    {
        float clientTime = g_engfuncs.GetClientTime();

        beam.modelIndex = modelIndex;
        beam.frameCount = model->numframes; // would break with studio models
        beam.source = start;
        beam.target = end;
        beam.delta = end - start;
        beam.freq = clientTime * speed;
        beam.die = clientTime + life;
        beam.width = width;
        beam.amplitude = amplitude;
        beam.speed = speed;
        beam.brightness = brightness;

        if (amplitude >= 0.5f)
        {
            beam.segments = (int)(VectorLength(beam.delta) * 0.25f + 3.0f);
        }
        else
        {
            beam.segments = (int)(VectorLength(beam.delta) * 0.075f + 3.0f);
        }
    }

    return beam;
}

static void DrawBeam(BEAM &beam, float framtime)
{
    // NYI
}

static void DrawEntity(cl_entity_t *entity, float frametime)
{
    float amplitude = (float)entity->curstate.body * 0.01f;
    float brightness = (float)entityUpdateRenderAmt(entity, g_state.viewOrigin, g_state.viewForward) / 255.0f;
    float speed = entity->curstate.animtime;

    BEAM beam = SetupBeam(
        entity->origin,
        entity->curstate.angles,
        entity->curstate.movetype,
        0.0f,
        entity->curstate.scale,
        amplitude,
        brightness,
        speed);

    beam.frame = (float)(int)entity->curstate.frame;
    beam.r = entity->curstate.rendercolor.r / 255.0f;
    beam.g = entity->curstate.rendercolor.g / 255.0f;
    beam.b = entity->curstate.rendercolor.b / 255.0f;

    // FIXME: no constant for the mask?
    int type = entity->curstate.rendermode & 0xF;

    if (type == BEAM_ENTPOINT)
    {
        beam.type = TE_BEAMPOINTS;
        beam.flags = FBEAM_ENDENTITY;
        beam.startEntity = 0;
        beam.endEntity = entity->curstate.skin;
    }
    else if (type == BEAM_ENTS)
    {
        beam.type = TE_BEAMPOINTS;
        beam.flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;
        beam.startEntity = entity->curstate.sequence;
        beam.endEntity = entity->curstate.skin;
    }

    int flags = entity->curstate.rendermode;

    if (flags & BEAM_FSINE)
    {
        beam.flags |= FBEAM_SINENOISE;
    }

    if (flags & BEAM_FSOLID)
    {
        beam.flags |= FBEAM_SOLID;
    }

    if (flags & BEAM_FSHADEIN)
    {
        beam.flags |= FBEAM_SHADEIN;
    }

    if (flags & BEAM_FSHADEOUT)
    {
        beam.flags |= FBEAM_SHADEOUT;
    }

    if (beam.modelIndex >= 0)
    {
        DrawBeam(beam, frametime);
    }
}

void beamDraw()
{
    int entityCount;
    cl_entity_t **entities = entityGetBeams(entityCount);
    if (!entityCount) /* would also check the shitty linked list here */
    {
        return;
    }

    // FIXME: won't work with older builds (hudGetClientOldTime doesn't exist)
    float frametime = g_engfuncs.GetClientTime() - g_engfuncs.hudGetClientOldTime();

    immediateDrawStart(false);
    immediateDepthMask(GL_FALSE);
    immediateCullFace(GL_FALSE);

    /* would process the linked list here (check modelindex < 0) */

    for (int i = 0; i < entityCount; i++)
    {
        DrawEntity(entities[i], frametime);
    }

    /* currently immediateDrawEnd resets state */
    immediateDrawEnd();
}

}
