#include "stdafx.h"
#include "lightstyle.h"
#include "internal.h" //LightmapSample... should be moved elsewhere

namespace Render
{

float g_lightstyles[MAX_LIGHTSTYLES];

void lightstyleUpdate()
{
    float time = g_engfuncs.GetClientTime() * 10;
    int step = static_cast<int>(time);
    float fraction = static_cast<float>(time - step);

    for (int i = 0; i < MAX_LIGHTSTYLES; i++)
    {
        const Lightstyle &style = platformLightstyleString(i);
        if (!style.size)
        {
            g_lightstyles[i] = 1.0f;
            continue;
        }

        char ch1 = style.data[step % style.size] - 'a';
        char ch2 = style.data[(step + 1) % style.size] - 'a';

        float value1 = ch1 * (22.0f / 256.0f);
        float value2 = ch2 * (22.0f / 256.0f);

        if (abs(ch1 - ch2) >= 3)
        {
            g_lightstyles[i] = value1;
        }
        else
        {
            g_lightstyles[i] = Lerp(value1, value2, fraction);
        }
    }

    GL3_ASSERT(g_lightstyles[NULL_LIGHTSTYLE] == 0);
}

void lightstyleReset()
{
    // set the value to 264 like the engine does...
    for (int i = 0; i < MAX_LIGHTSTYLES; i++)
    {
        g_lightstyles[i] = (264.0f / 255.0f);
    }
}

Vector3 lightstyleApply(const LightmapSamples &samples)
{
    Vector3 result{};

    for (int j = 0; j < MAXLIGHTMAPS; j++)
    {
        int style = samples.samples[j].style;
        if (style >= MAX_LIGHTSTYLES)
        {
            break;
        }

        float weight = g_lightstyles[style];
        result.x += samples.samples[j].r * weight;
        result.y += samples.samples[j].g * weight;
        result.z += samples.samples[j].b * weight;
    }

    result.x = Q_min(result.x, 255.0f);
    result.y = Q_min(result.y, 255.0f);
    result.z = Q_min(result.z, 255.0f);

    return result;
}

}
