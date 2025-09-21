#include "stdafx.h"
#include "water.h"
#include "brush.h"
#include "commandbuffer.h"

namespace Render
{

class WaterShader final : public BaseShader
{
public:
    const char *Name()
    {
        return "water";
    }

    const VertexAttrib *VertexAttribs()
    {
        return g_brushVertexFormat.attribs;
    }

    const ShaderUniform *Uniforms()
    {
        static const ShaderUniform uniforms[] = {
            SHADER_UNIFORM_CONST(u_texture, 0),
            SHADER_UNIFORM_TERM()
        };

        return uniforms;
    }
};

static WaterShader s_shader;

void waterInit()
{
    shaderRegister(s_shader);
}

void waterDrawBegin()
{
    // can't remember why this was done but ok
    commandCullFace(GL_FALSE);

    commandUseProgram(&s_shader);
}

void waterDrawEnd()
{
    // can't remember why this was done but ok
    commandCullFace(GL_TRUE);
}

}
