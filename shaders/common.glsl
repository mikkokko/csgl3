// include file shared between all vertex and pixel shaders

// c++ defs
#include "shader_common.h"

#define M_PI 3.14159265358979323846

// needs to match the c++ code
layout(std140) uniform FrameConstants
{
    mat4 viewProjectionMatrix;
    mat4 skyMatrix; // sky is rendered every frame so leave this here for now
    mat4 vmViewProjectionMatrix; // viewmodel is rendered every frame so leave this here for now
    vec4 cameraRight; // only used by studio model chrome? why have it here

    vec4 clientTime; // FIXME: could pack with... something

    vec4 lightPositions[MAX_SHADER_LIGHTS]; // w stores 1/radius
    vec4 lightColors[MAX_SHADER_LIGHTS];

    // packed, accessed with lightstyles[i / 4][i % 4]
    vec4 lightstyles[MAX_LIGHTSTYLES / 4];
};

// used by brush and studio shaders
const float k_brightness = V_BRIGHTNESS;
const float k_gamma = V_GAMMA;
const float k_lightgamma = V_LIGHTGAMMA;
const float k_brighten = 0.125 - clamp(V_BRIGHTNESS * V_BRIGHTNESS, 0.0, 1.0) * 0.075;

// constant buffer for all things fog
layout(std140) uniform FogConstants
{
    // rgb color
    vec4 fogColor;

    // x: -density*density*log2(e)
    // y: skybox fog factor
    vec4 fogParams;
};

#define fogExp2Param fogParams.x
#define skyboxFogFactor fogParams.y

float FogFactor(float fogCoord)
{
    float fogFactor = exp2(fogExp2Param * fogCoord * fogCoord);    
    return clamp(fogFactor, 0.0, 1.0);
}

// software style overbright
#define OVERBRIGHT 0
