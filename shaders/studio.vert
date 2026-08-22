#include "common.glsl"

in vec3 a_position;
in vec3 a_normal;
in vec2 a_texCoord;
in float a_bone;

#if defined(GLOWSHELL)
in vec3 a_smoothNormal;
#endif

// per studio model, must match c++ code
layout(std140) uniform ModelConstants
{
    vec4 renderColor;
    vec4 renderColorLinear; // only used by the elight path
    vec4 lightDir;
    vec4 ambientAndShadeLight; // x = ambientlight, y = shadelight
    vec4 chromeOriginAndShellScale; // chrome origin (xyz) and glowshell scale (w)

    // room for 4 lights, but only 3 used
    vec4 elights[3]; // x,y,z
    vec4 elightColors[3]; // r,g,b

    // boolean constants...
    bool viewmodel;
    bool additive;
};

layout(std140) uniform BoneConstants
{
    mat3x4 bones[MAX_SHADER_BONES];
};

layout(std140) uniform TextureConstants
{
    // only used for glowshell
    vec2 uvScale;

    // not used for glowshell
    // separate bools for GLSL 1.20
    bool fullbright;
    bool flatshade;
    bool chrome;
};

#if defined(GLOWSHELL)
// make these constants for glowshell so we don't
// completely clutter the code with ifdefs
#define fullbright false
#define flatshade false
#define chrome true
#endif

// awful packing
#define ambientLight ambientAndShadeLight.x
#define shadeLight ambientAndShadeLight.y
#define chromeOrigin chromeOriginAndShellScale.xyz
#define shellScale chromeOriginAndShellScale.w

out vec2 f_texCoord;
out float f_fogFactor;
out vec4 f_color;

// engine's v_lambert1, doesn't change
const float k_lambert = 1.4953241;

vec2 ChromeTexCoords(mat3x4 bone, vec3 normal)
{
    vec3 pos = vec3(bone[0].w, bone[1].w, bone[2].w);

    vec3 forward = normalize(pos - chromeOrigin);
    vec3 up = normalize(cross(forward, cameraRight));
    vec3 side = cross(forward, up);

    vec2 texCoords;
    texCoords.x = 0.5 - 0.5 * dot(normal, side);
    texCoords.y = 0.5 + 0.5 * dot(normal, up);

    return texCoords;
}

#if defined(HAS_ELIGHTS)
vec3 AddElights(vec3 position, vec3 normal)
{
    vec3 dx = elights[0].xyz - position.x;
    vec3 dy = elights[1].xyz - position.y;
    vec3 dz = elights[2].xyz - position.z;

    vec3 dist2 = dx * dx;
    dist2 += dy * dy;
    dist2 += dz * dz;

    vec3 r = inversesqrt(dist2);
    vec3 atten = r * r * r;

    vec3 NdotL = dx * normal.x;
    NdotL += dy * normal.y;
    NdotL += dz * normal.z;

    vec3 w = max(NdotL, vec3(0.0)) * atten;

    return vec3(
        dot(w, elightColors[0].rgb),
        dot(w, elightColors[1].rgb),
        dot(w, elightColors[2].rgb));
}

float ApplyBrightnessLinear(float value)
{
    float f = pow(value, k_lightgamma) * max(k_brightness, 1.0);
    float a = (f / k_brighten) * 0.125;
    float b = (f - k_brighten) / (1.0 - k_brighten) * 0.875 + 0.125;
    return min(mix(a, b, step(k_brighten, f)), 1.0);
}
#endif

vec4 ComputeColor(vec3 position, vec3 normal)
{
    if (additive)
    {
        // color as-is for additive
        return renderColor;
    }

    if (fullbright)
    {
        // no lighting, alpha as-is
        return vec4(1.0, 1.0, 1.0, renderColor.a);
    }

    float diffuse;
    if (flatshade)
    {
        diffuse = 0.8;
    }
    else
    {
        // assumes that k_lambert >= 1.0
        float NdotL = dot(normal, lightDir.xyz);
        diffuse = (1.0 - NdotL) * (1.0 / k_lambert);
        diffuse = min(diffuse, 1.0);
    }

    diffuse = ambientLight + (shadeLight * diffuse);

#if defined(HAS_ELIGHTS)
    vec3 linear = renderColorLinear.rgb * ApplyBrightnessLinear(diffuse);
    linear += AddElights(position, normal);
    vec3 color = pow(min(linear, vec3(1.0)), vec3(1.0 / k_gamma));
#else
    vec3 color = renderColor.rgb * ApplyBrightness(diffuse);
#endif

#if defined(OVERBRIGHT)
    color *= (255.0 / 192.0);
#endif

    return vec4(color, renderColor.a);
}

void main()
{
    mat3x4 bone = bones[int(a_bone)];
	mat3x3 boneRot = mat3(bone);

    vec3 position = vec4(a_position, 1.0) * bone;
    vec3 normal = normalize(a_normal * boneRot);

    f_texCoord = chrome ? ChromeTexCoords(bone, normal) : a_texCoord;

	// glowshell bullshit
#if defined(GLOWSHELL)
	// apply the texcoord scale kludge
	f_texCoord *= uvScale;

	// woah, shell effect
	// FIXME: normalize this without NaNs???
	position += normalize(a_smoothNormal * boneRot) * shellScale;
#endif

#if defined(GLOWSHELL)
    f_color = renderColor;
#else
    f_color = ComputeColor(position, normal);
#endif

    mat4 viewProj = viewmodel ? vmViewProjectionMatrix : viewProjectionMatrix;

    gl_Position = viewProj * vec4(position, 1.0);

    f_fogFactor = FogFactor(gl_Position.w);
}
