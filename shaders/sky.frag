#include "common.glsl"
#include "brush_common.glsl"

uniform samplerCube u_texture;

in vec3 f_texCoord;

out vec4 fragColor;

void main()
{
    fragColor = texture(u_texture, f_texCoord);
    fragColor.rgb = mix(fogColor.rgb, fragColor.rgb, skyboxFogFactor);
}
