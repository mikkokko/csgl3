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

    // xyz = cameraRight (only used by studio model chrome)
    // w = clientTime
    vec4 cameraRightAndClientTime;

    // 4 lights
    vec4 dlights[4]; // x,y,z,1/radius^2
    vec4 dlightColors[3]; // r,g,b

    // accessed with lightstyles[i].x
    vec4 lightstyles[MAX_LIGHTSTYLES];
};

#define cameraRight cameraRightAndClientTime.xyz
#define clientTime cameraRightAndClientTime.w

// used by brush and studio shaders
const float k_brightness = V_BRIGHTNESS;
const float k_gamma = V_GAMMA;
const float k_lightgamma = V_LIGHTGAMMA;
// need to clamp before squaring to match the engine
const float k_brightnessClamped = clamp(V_BRIGHTNESS, 0.0, 1.0);
const float k_brighten = 0.125 - k_brightnessClamped * k_brightnessClamped * 0.075;

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
