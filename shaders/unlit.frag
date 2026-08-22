#include "common.glsl"
#include "brush_common.glsl"

uniform sampler2D u_texture;

#if defined(DETAIL)
uniform sampler2D u_detail;
#endif

in vec2 texCoord;
in float f_fogFactor;

out vec4 fragColor;

void main()
{
    fragColor = texture(u_texture, texCoord) * renderColor;

#if defined(DETAIL)
    vec3 detail = texture(u_detail, texCoord.xy * detailScale).rgb;
    fragColor.rgb *= detail * 2.0;
#endif

    fragColor.rgb = mix(fogColor.rgb, fragColor.rgb, f_fogFactor);
}
