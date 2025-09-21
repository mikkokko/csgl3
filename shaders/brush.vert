#include "common.glsl"
#include "brush_common.glsl"

// note: lightmap width is stored in a_position.w (temp test)
in vec4 a_position;
in vec4 a_texCoord;
in vec4 a_styles;

uniform float u_scroll;

out vec3 fragPosition;
out vec4 texCoord;

flat out vec4 f_lightmapWeights;
flat out float f_lightmapWidth;

out float f_fogFactor;

void main()
{
    texCoord = a_texCoord;
    texCoord.x += u_scroll;

#if !NO_LIGHTING
    ivec4 styles = ivec4(a_styles);
    ivec4 index1 = styles >> 2;
    ivec4 index2 = styles & 3;

    f_lightmapWeights.x = lightstyles[index1.x][index2.x];
    f_lightmapWeights.y = lightstyles[index1.y][index2.y];
    f_lightmapWeights.z = lightstyles[index1.z][index2.z];
    f_lightmapWeights.w = lightstyles[index1.w][index2.w];
    f_lightmapWidth = a_position.w;
#endif

    vec3 position = vec4(a_position.xyz, 1.0) * modelMatrix;
    fragPosition = position;

    gl_Position = viewProjectionMatrix * vec4(position, 1.0);

    f_fogFactor = FogFactor(gl_Position.w);
}
