#include "common.glsl"
#include "studio_common.glsl"

in vec3 a_position;
in vec3 a_normal;
in vec2 a_texCoord;
in float a_bone;

uniform bool u_viewmodel; // FIXME
uniform int u_flags;

// FIXME: vertex texture fetch considered harmul on shitty cards
//uniform sampler1D u_lightgammaLut;

out vec2 f_texCoord;
out float f_fogFactor;
out vec4 f_color;

// engine's v_lambert1, doesn't change
const float k_lambert = 1.4953241;

// FIXME: try cleaning up if viewproj matrix gets split
vec2 ChromeTexCoords(mat3x4 bone, vec3 normal)
{
    vec3 pos = vec3(bone[0].w, bone[1].w, bone[2].w);

    vec3 forward = normalize(pos - chromeOrigin);

    vec3 up = normalize(cross(forward, cameraRight.xyz));
    vec3 side = normalize(cross(forward, up));

    vec2 texCoords;
    texCoords.x = 0.5 - 0.5 * dot(normal, side);
    texCoords.y = 0.5 + 0.5 * dot(normal, up);

    return texCoords;
}

vec3 ApplyElights(vec3 srgb, vec3 position, vec3 normal)
{
    vec3 elights = vec3(0.0);

    for(int i = 0; i < STUDIO_MAX_ELIGHTS; i++)
    {
        // NOTE: not normalized
        vec3 direction = elightPositions[i].xyz - position;

        // wtf is this attenuation
        float magnitudeSquared = dot(direction, direction);
        float radiusSquared = elightColors[i].w;

        float magrsqrt = inversesqrt(magnitudeSquared);
        float attenuation = radiusSquared * (magrsqrt * magrsqrt * magrsqrt);

        float NdotL = max(dot(normal, direction), 0.0);
        elights += elightColors[i].rgb * NdotL * attenuation;
    }

    vec3 linear = pow(srgb, vec3(k_gamma));
    linear += elights;
    return min(pow(linear, vec3(1.0 / k_gamma)), vec3(1.0));
}

// the reason this uses mix and step is that it used to take a vec3
float Brighten(float f)
{
    float a = (f / k_brighten) * 0.125;
    float b = (f - k_brighten) / (1.0 - k_brighten) * 0.875 + 0.125;
    return mix(a, b, step(k_brighten, f));
}

float ApplyBrightness(float value)
{
    value = pow(value, float(k_lightgamma));
    value *= max(k_brightness, 1.0);
    value = Brighten(value);
    return pow(value, float(1.0 / k_gamma));
}

vec3 StudioLighting(vec3 position, vec3 normal)
{
    float diffuse;
    if ((u_flags & STUDIO_SHADER_FLATSHADE) != 0)
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
    diffuse = min(ApplyBrightness(diffuse), 1.0);

    vec3 result = renderColor.rgb * diffuse;

    if ((u_flags & STUDIO_SHADER_ELIGHTS) != 0)
    {
        result = ApplyElights(result, position, normal);
    }

    return result;
}

vec4 ComputeColor(vec3 position, vec3 normal)
{
    if ((u_flags & STUDIO_SHADER_COLOR_ONLY) != 0)
    {
        // color as-is
        return renderColor;
    }

    vec4 result;

    // compute lighting, alpha as-is
    bool fullbright = (u_flags & STUDIO_SHADER_FULLBRIGHT) != 0;
    result.rgb = fullbright ? vec3(1.0, 1.0, 1.0) : StudioLighting(position, normal);
    result.a = renderColor.a;

    return result;
}

void main()
{
    mat3x4 bone = bones[int(a_bone)];

    vec3 position = vec4(a_position, 1.0) * bone;
    vec3 normal = normalize(a_normal * mat3(bone));

    // shell effect
    position += normal * shellScale;

    bool chrome = (u_flags & STUDIO_SHADER_CHROME) != 0;
    f_texCoord = chrome ? ChromeTexCoords(bone, normal) : a_texCoord;

    f_color = ComputeColor(position, normal);

    mat4 viewProj = u_viewmodel ? vmViewProjectionMatrix : viewProjectionMatrix;

    gl_Position = viewProj * vec4(position, 1.0);

    f_fogFactor = FogFactor(gl_Position.w);
}
