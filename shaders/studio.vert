#include "common.glsl"
#include "studio_common.glsl"

in vec3 a_position;
in vec3 a_normal;
in vec2 a_texCoord;
//in vec4 a_tangent;
in float a_bone;

uniform bool u_viewmodel; // FIXME
uniform int u_flags;

out vec2 f_texCoord;
out vec3 f_position;
out vec3 f_normal;

out float f_fogFactor;

vec2 ChromeTexCoords(mat3x4 bone, vec3 normal)
{
    mat4x3 boneTranspose = transpose(bone);

    vec3 forward = normalize(boneTranspose[3] - chromeOrigin);

    vec3 up = normalize(cross(forward, cameraRight.xyz));
    vec3 side = normalize(cross(forward, up));

    vec2 texCoords;
    texCoords.x = 1.0 - (dot(normal, side) + 1.0) * 0.5;
    texCoords.y = (dot(normal, up) + 1.0) * 0.5;

    return texCoords;
}

void main()
{
    mat3x4 bone = bones[int(a_bone)];

    vec3 position = vec4(a_position, 1.0) * bone;
    vec3 normal = normalize(a_normal * mat3(bone));

    bool chrome = (u_flags & STUDIO_SHADER_CHROME) != 0;
    f_texCoord = chrome ? ChromeTexCoords(bone, normal) : a_texCoord;

    // shell effect
    position += normal * shellScale;

    f_position = position;
    f_normal = normal;

    mat4 viewProj = u_viewmodel ? vmViewProjectionMatrix : viewProjectionMatrix;

    gl_Position = viewProj * vec4(position, 1.0);

    f_fogFactor = FogFactor(gl_Position.w);
}
