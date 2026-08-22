// halflife.glsl - goldsrc ubershader
#include "common.glsl"
#include "brush_common.glsl"

uniform sampler2D u_texture;
uniform sampler2D u_lightmap;

#if defined(DETAIL)
uniform sampler2D u_detail;
#endif

in vec3 fragPosition;
in vec4 texCoord;

in vec4 f_lightmapWeights;
in float f_lightmapWidth;

in float f_fogFactor;

out vec4 fragColor;

// not accurate, but convincing enough
// the x1600 hated this, so this was made unreadable to make it happy
#if defined(HAS_DLIGHTS)
vec3 AddDLights()
{
    vec4 dx = fragPosition.x - dlights[0];
    vec4 dy = fragPosition.y - dlights[1];
    vec4 dz = fragPosition.z - dlights[2];

    vec4 dist2 = dx * dx;
    dist2 += dy * dy;
    dist2 += dz * dz;

    vec4 atten = max(vec4(0.0), 1.0 - dist2 * dlights[3]);

    return vec3(
        dot(atten, dlightColors[0]),
        dot(atten, dlightColors[1]),
        dot(atten, dlightColors[2]));
}
#endif

void main()
{
    // discard might turn off early z, so we have it as a shader variant
    vec4 diffuse = texture(u_texture, texCoord.xy);
#if defined(ALPHA_TEST)
    if (diffuse.a < 0.25)
    {
        discard;
        return;
    }
#endif

#if defined(DETAIL)
    vec3 detail = texture(u_detail, texCoord.xy * detailScale).rgb;
    diffuse.rgb *= detail * 2.0;
#endif

    vec3 lightmap = f_lightmapWeights[0] * texture(u_lightmap, texCoord.zw).rgb;

#if defined(MULTI_STYLE)
    vec2 uvOffset = vec2(f_lightmapWidth, 0.0);
    lightmap += f_lightmapWeights[1] * texture(u_lightmap, texCoord.zw + uvOffset * 1.0).rgb;
    lightmap += f_lightmapWeights[2] * texture(u_lightmap, texCoord.zw + uvOffset * 2.0).rgb;
    lightmap += f_lightmapWeights[3] * texture(u_lightmap, texCoord.zw + uvOffset * 3.0).rgb;
#endif

#if defined(HAS_DLIGHTS)
    lightmap += AddDLights();
#endif

#if !defined(OVERBRIGHT)
    lightmap *= V_LIGHTGAMMA_2X;
#endif

    lightmap = ApplyBrightness(lightmap);

#if defined(OVERBRIGHT)
    lightmap *= (255.0 / 192.0);
#endif

    diffuse.rgb *= lightmap;

    diffuse.rgb = mix(fogColor.rgb, diffuse.rgb, f_fogFactor);

    fragColor = diffuse;
}
