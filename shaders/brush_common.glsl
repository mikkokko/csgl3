// constant buffer for brush and water shaders

// needs to match the c++ code
layout(std140) uniform ModelConstants
{
    mat3x4 modelMatrix;
    vec4 renderColor;
};
