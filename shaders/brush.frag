// halflife.glsl - goldsrc ubershader
#include "common.glsl"
#include "brush_common.glsl"

uniform sampler2D u_texture;
uniform sampler2D u_lightmap;

uniform sampler1D u_lightgammaLut;

in vec3 fragPosition;
in vec4 texCoord;

flat in vec4 f_lightmapWeights;
flat in float f_lightmapWidth;

in float f_fogFactor;

out vec4 fragColor;

// made this the fuck up, completely wrong and based on nothing
vec3 AddLight(vec3 pos, float invRadius, vec3 color)
{
    float dist = distance(pos, fragPosition);
    float attenuation = max(0.0, 1.0 - dist * invRadius);
    return color * attenuation * 0.3; // wtf is 0.3? no idea
}

vec3 ApplyBrightnessLUT(vec3 color)
{
    return vec3(
        texture(u_lightgammaLut, color.r).r,
        texture(u_lightgammaLut, color.g).r,
        texture(u_lightgammaLut, color.b).r);
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

    // software style overbrights
    lightmap = ApplyBrightnessLUT(lightmap) * (255.0 / 192.0);

    diffuse.rgb *= lightmap;
#else
    diffuse *= renderColor;
#endif

    diffuse.rgb = mix(fogColor.rgb, diffuse.rgb, f_fogFactor);

    fragColor = diffuse;
}
