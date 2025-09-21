#include "common.glsl"
#include "brush_common.glsl"

uniform sampler2D u_texture;

in vec4 f_texCoord;

in float f_fogFactor;

out vec4 fragColor;

void main()
{
    // quake style
    vec2 texCoord = f_texCoord.xy;
    texCoord += sin(f_texCoord.yx + clientTime.x) * 0.125;

    vec4 diffuse = texture(u_texture, texCoord);

    diffuse.rgb *= renderColor.rgb;
    diffuse.a = renderColor.a;

    diffuse.rgb = mix(fogColor.rgb, diffuse.rgb, f_fogFactor);

    fragColor = diffuse;
}
