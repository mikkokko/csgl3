#include "common.glsl"
#include "brush_common.glsl"

// note: lightmap width is stored in a_position.w (temp test)
in vec4 a_position;
in vec2 a_texCoord;
in vec2 a_lightmapTexCoord;
in vec4 a_styles;

uniform float u_scroll;

out vec3 fragPosition;
out vec4 texCoord;

flat out vec4 f_lightmapWeights;
flat out float f_lightmapWidth;

out float f_fogFactor;

void main()
{
    texCoord = vec4(a_texCoord, a_lightmapTexCoord);
    texCoord.x += u_scroll;

    uvec4 styles = uvec4(a_styles);
    f_lightmapWeights.x = lightstyles[styles.x].x;
    f_lightmapWeights.y = lightstyles[styles.y].x;
    f_lightmapWeights.z = lightstyles[styles.z].x;
    f_lightmapWeights.w = lightstyles[styles.w].x;
    f_lightmapWidth = a_position.w;

    vec3 position = vec4(a_position.xyz, 1.0) * modelMatrix;
    fragPosition = position;

    gl_Position = viewProjectionMatrix * vec4(position, 1.0);

    f_fogFactor = FogFactor(gl_Position.w);
}
