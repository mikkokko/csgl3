#include "common.glsl"
#include "studio_common.glsl"

uniform sampler2D u_texture;
uniform sampler2D u_bumpmap;

uniform int u_flags;

in vec2 f_texCoord;
in vec3 f_normal;
in vec3 f_lightDirs[STUDIO_MAX_ELIGHTS];
in float f_fogFactor;

out vec4 fragColor;

// engine's v_lambert1, doesn't change
const float k_lambert = 1.4953241;

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

vec3 ApplyElights(vec3 srgb, vec3 normal)
{
    vec3 elights = vec3(0.0);

    for(int i = 0; i < STUDIO_MAX_ELIGHTS; i++)
    {
        // NOTE: not normalized
        vec3 direction = f_lightDirs[i];
        float NdotL = max(dot(normal, direction), 0.0);

        // wtf is this attenuation
        float magnitudeSquared = dot(direction, direction);
        float radiusSquared = elightColors[i].w;

        float magrsqrt = inversesqrt(magnitudeSquared);
        float attenuation = radiusSquared * (magrsqrt * magrsqrt * magrsqrt);

        elights += elightColors[i].rgb * NdotL * attenuation;
    }

    vec3 linear = pow(srgb, vec3(k_gamma));
    linear += elights;
    return pow(linear, vec3(1.0 / k_gamma));
}

vec3 StudioLighting()
{
    vec3 normal = normalize(f_normal);

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
    diffuse = ApplyBrightness(diffuse);

#if (OVERBRIGHT == 0)
    diffuse = min(diffuse, 1.0);
#endif

    vec3 result = renderColor.rgb * diffuse;

    if ((u_flags & STUDIO_SHADER_ELIGHTS) != 0)
    {
        result = ApplyElights(result, normal);
    }

    return result;
}

vec4 ComputeColor()
{
    if ((u_flags & STUDIO_SHADER_COLOR_ONLY) != 0)
    {
        // color as-is
        return renderColor;
    }

    vec4 result;

    // compute lighting, alpha as-is
    bool fullbright = (u_flags & STUDIO_SHADER_FULLBRIGHT) != 0;
    result.rgb = fullbright ? vec3(1.0, 1.0, 1.0) : StudioLighting();
    result.a = renderColor.a;

    return result;
}

void main()
{
    // discard might turn off early z, so we have it as a shader variant
    // NOTE: to match engine results, we should compute the final color first and then test, but fuck that (looks ugly)
    vec4 albedo = texture(u_texture, f_texCoord);
#if ALPHA_TEST
    if (albedo.a < 0.5)
    {
        discard;
        return;
    }
#endif

    fragColor = albedo * ComputeColor();

    fragColor.rgb = mix(fogColor.rgb, fragColor.rgb, f_fogFactor);
}
