#include "common.glsl"

uniform sampler2D u_texture;

in vec4 f_color;
in vec2 f_texCoord;

in float f_fogFactor;

out vec4 fragColor;

// god i hate these fucking games
const float k_alphaRef = 1.0 / 255.0;
    
void main()
{
    fragColor = texture(u_texture, f_texCoord) * f_color;

    // discard might turn off early z, so we have it as a shader variant
#if defined(ALPHA_TEST)
    if (fragColor.a < k_alphaRef)
    {
        discard;
        return;
    }
#endif

    fragColor.rgb = mix(fogColor.rgb, fragColor.rgb, f_fogFactor);
}
