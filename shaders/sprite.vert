#include "common.glsl"

in vec3 a_position;
in vec2 a_texCoord;
in vec4 a_color;

out vec4 f_color;
out vec2 f_texCoord;
out float f_fogFactor;


void main()
{
    f_color = a_color;
    f_texCoord = a_texCoord;
    gl_Position = viewProjectionMatrix * vec4(a_position, 1.0);

    f_fogFactor = FogFactor(gl_Position.w);
}
