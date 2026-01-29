#include "stdafx.h"
#include "gamma.h"

namespace Render
{

static cvar_t *v_brightness;
static cvar_t *v_gamma;
static cvar_t *v_lightgamma;
static cvar_t *v_texgamma;

static float s_brightness;
static float s_gamma;
static float s_lightgamma;
static float s_texgamma;

byte g_gammaTextureTable[256];
byte g_gammaLinearTable[256];
static byte s_gammaLightTable[256];

static void OnVariableChanged()
{
    float brightness = v_brightness->value;
    float gamma = v_gamma->value;
    float lightgamma = v_lightgamma->value;
    float texgamma = v_texgamma->value;

    s_brightness = brightness;
    s_gamma = gamma;
    s_lightgamma = lightgamma;
    s_texgamma = texgamma;

    float invgamma = 1.0f / gamma;
    float brighten = 0.125f - Q_clamp(brightness * brightness, 0.0f, 1.0f) * 0.075f;

    for (int i = 0; i < 256; i++)
    {
        const float f = static_cast<float>(i) / 255.0f;

        {
            float texture = powf(f, invgamma * texgamma) * 255.0f;
            texture = Q_clamp(texture, 0.0f, 255.0f);
            g_gammaTextureTable[i] = static_cast<byte>(texture);
        }

        {
            float linear = powf(f, gamma) * 255.0f;
            linear = Q_clamp(linear, 0.0f, 255.0f);
            g_gammaLinearTable[i] = static_cast<byte>(linear);
        }

        {
            float light = powf(f, lightgamma);
            light *= Q_max(brightness, 1.0f);

            if (light > brighten)
            {
                light = 0.125f + ((light - brighten) / (1.0f - brighten)) * 0.875f;
            }
            else
            {
                light = (light / brighten) * 0.125f;
            }

            light = powf(light, 1.0f / gamma) * 255.0f;
            light = Q_clamp(light, 0.0f, 255.0f);
            s_gammaLightTable[i] = static_cast<byte>(light);
        }
    }

    shaderUpdateGamma(brightness, gamma, lightgamma);
}

void gammaInit()
{
    v_brightness = g_engfuncs.pfnGetCvarPointer("brightness");
    v_gamma = g_engfuncs.pfnGetCvarPointer("gamma");
    v_lightgamma = g_engfuncs.pfnGetCvarPointer("lightgamma");
    v_texgamma = g_engfuncs.pfnGetCvarPointer("texgamma");

    OnVariableChanged();
}

void gammaUpdate()
{
    // check for cvar change
    if (s_brightness != v_brightness->value
        || s_gamma != v_gamma->value
        || s_lightgamma != v_lightgamma->value
        || s_texgamma != v_texgamma->value)
    {
        OnVariableChanged();
    }
}

}
