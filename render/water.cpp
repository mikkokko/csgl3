#include "stdafx.h"
#include "water.h"
#include "brush.h"
#include "commandbuffer.h"

namespace Render
{

static const ShaderUniform s_uniforms[] = {
    { "u_texture", 0 }
};

static BaseShader s_shader;

void waterInit()
{
    shaderRegister(s_shader, "water", g_brushVertexFormat.attribs, s_uniforms);
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
