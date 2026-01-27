#include "common.glsl"
#include "studio_common.glsl"

uniform sampler2D u_texture;

uniform int u_flags;

in vec2 f_texCoord;
in float f_fogFactor;
in vec4 f_color;

out vec4 fragColor;

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

    fragColor = albedo * f_color;

    fragColor.rgb = mix(fogColor.rgb, fragColor.rgb, f_fogFactor);
}
