// halflife.glsl - goldsrc ubershader
#include "common.glsl"
#include "brush_common.glsl"

uniform sampler2D u_texture;
uniform sampler2D u_lightmap;

in vec3 fragPosition;
in vec4 texCoord;

flat in vec4 f_lightmapWeights;
flat in float f_lightmapWidth;

in float f_fogFactor;

out vec4 fragColor;

vec3 Brighten(vec3 f)
{
    vec3 a = (f / k_brighten) * 0.125;
    vec3 b = (f - k_brighten) / (1.0 - k_brighten) * 0.875 + 0.125;
    return mix(a, b, step(k_brighten, f));
}

vec3 ApplyBrightness(vec3 value)
{
    value = pow(value, vec3(k_lightgamma));
    value *= 2.0;
    value *= max(k_brightness, 1.0);
    value = Brighten(value);
    value = pow(value, vec3(1.0 / k_gamma));
    return value;
}

// made this the fuck up, completely wrong and based on nothing
vec3 AddLight(vec3 pos, float invRadius, vec3 color)
{
    float dist = distance(pos, fragPosition);
    float attenuation = max(0.0, 1.0 - dist * invRadius);
    return color * attenuation * 0.3; // wtf is 0.3? no idea
}

void main()
{
    // discard might turn off early z, so we have it as a shader variant
    vec4 diffuse = texture(u_texture, texCoord.xy);
#if ALPHA_TEST
    if (diffuse.a < 0.25)
    {
        discard;
        return;
    }
#endif

#if !NO_LIGHTING
    vec2 uvOffset = vec2(f_lightmapWidth, 0.0);

    vec3 lightmap = f_lightmapWeights[0] * texture(u_lightmap, texCoord.zw).rgb;

    lightmap += f_lightmapWeights[1] * texture(u_lightmap, texCoord.zw + uvOffset * 1.0).rgb;
    lightmap += f_lightmapWeights[2] * texture(u_lightmap, texCoord.zw + uvOffset * 2.0).rgb;
    lightmap += f_lightmapWeights[3] * texture(u_lightmap, texCoord.zw + uvOffset * 3.0).rgb;

    for (int i = 0; i < MAX_SHADER_LIGHTS; i++)
    {
        lightmap += AddLight(lightPositions[i].xyz,
            lightPositions[i].w,
            lightColors[i].rgb);
    }

    lightmap = ApplyBrightness(lightmap);

#if (OVERBRIGHT == 0)
    lightmap = min(lightmap, 1.0);
#endif

    diffuse.rgb *= lightmap;
#else
    diffuse *= renderColor;
#endif

    diffuse.rgb = mix(fogColor.rgb, diffuse.rgb, f_fogFactor);

    fragColor = diffuse;
}
