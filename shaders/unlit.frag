#include "common.glsl"
#include "brush_common.glsl"

uniform sampler2D u_texture;

in vec2 texCoord;
in float f_fogFactor;

out vec4 fragColor;

void main()
{
    fragColor = texture(u_texture, texCoord) * renderColor;
    fragColor.rgb = mix(fogColor.rgb, fragColor.rgb, f_fogFactor);
}
