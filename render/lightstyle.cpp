#include "stdafx.h"
#include "lightstyle.h"

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

}
