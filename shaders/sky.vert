#include "common.glsl"
#include "brush_common.glsl"

in vec3 a_position;

out vec3 f_texCoord;

void main()
{
    f_texCoord = (skyMatrix * vec4(a_position, 1.0)).xyz;
    vec3 position = vec4(a_position.xyz, 1.0) * modelMatrix;
    gl_Position = viewProjectionMatrix * vec4(position, 1.0);
    gl_Position.z = gl_Position.w;
}
