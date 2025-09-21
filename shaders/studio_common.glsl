// per studio model, must match c++ code
layout(std140) uniform ModelConstants
{
    vec4 renderColor;
    vec4 lightDir;
    vec4 ambientAndShadeLight; // x = ambientlight, y = shadelight
    vec4 chromeOriginAndShellScale; // chrome origin (xyz) and glowshell scale (w)

    vec4 elightPositions[STUDIO_MAX_ELIGHTS]; // 4th component stores radius^2
    vec4 elightColors[STUDIO_MAX_ELIGHTS];

    mat3x4 bones[MAX_SHADER_BONES];
};

// awful packing
#define ambientLight ambientAndShadeLight.x
#define shadeLight ambientAndShadeLight.y
#define chromeOrigin chromeOriginAndShellScale.xyz
#define shellScale chromeOriginAndShellScale.w
