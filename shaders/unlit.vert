#include "common.glsl"
#include "brush_common.glsl"

in vec4 a_position;
in vec2 a_texCoord;

uniform float u_scroll;

out vec2 texCoord;
out float f_fogFactor;

void main()
{
    texCoord = a_texCoord;
    texCoord.x += u_scroll;

    vec3 position = vec4(a_position.xyz, 1.0) * modelMatrix;
    gl_Position = viewProjectionMatrix * vec4(position, 1.0);

    f_fogFactor = FogFactor(gl_Position.w);
}
