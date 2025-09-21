#include "common.glsl"
#include "brush_common.glsl"

in vec3 a_position;
in vec4 a_texCoord;

out vec4 f_texCoord;
out float f_fogFactor;

void main()
{
    f_texCoord = a_texCoord;

    vec3 position = vec4(a_position.xyz, 1.0) * modelMatrix;
    gl_Position = viewProjectionMatrix * vec4(position, 1.0);

    f_fogFactor = FogFactor(gl_Position.w);
}
