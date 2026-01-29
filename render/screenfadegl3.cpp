#include "stdafx.h"
#include "screenfadegl3.h"

namespace Render
{

struct ScreenFadeShader : BaseShader
{
    GLint u_color;
};

static const ShaderUniform s_uniforms[] = {
    { "u_color", &ScreenFadeShader::u_color }
};

static ScreenFadeShader s_shader;

void screenFadeInit()
{
    shaderRegister(s_shader, "screenfade", {}, {});
}

static float ComputeAlpha(screenfade_t &screenFade)
{
    float time = g_engfuncs.GetClientTime();

    if (!(screenFade.fadeFlags & FFADE_STAYOUT))
    {
        if (time > screenFade.fadeReset && time > screenFade.fadeEnd)
        {
            return 0.0f;
        }

        float result = (screenFade.fadeEnd - time) * screenFade.fadeSpeed;
        if (screenFade.fadeFlags & FFADE_OUT)
        {
            result += screenFade.fadealpha;
        }

        return Q_clamp(result, 0.0f, (float)screenFade.fadealpha);
    }

    if ((screenFade.fadeFlags & FFADE_OUT) && screenFade.fadeTotalEnd > time)
    {
        float result = screenFade.fadealpha + (screenFade.fadeTotalEnd - time) * screenFade.fadeSpeed;
        return Q_clamp(result, 0.0f, (float)screenFade.fadealpha);
    }
    else
    {
        screenFade.fadeEnd = time + 0.1f;
        return screenFade.fadealpha;
    }
}

void screenFadeDraw()
{
    screenfade_t screenFade{};
    g_engfuncs.pfnGetScreenFade(&screenFade);

    float alpha = ComputeAlpha(screenFade);
    if (!alpha)
    {
        return;
    }

    // we're not using the shitty command buffer stuff for screenfade, it gets drawn after that
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    Vector4 color;
    if (screenFade.fadeFlags & FFADE_MODULATE)
    {
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        float fraction = alpha / 255.0f;
        color.x = Lerp(screenFade.fader / 255.0f, 1.0f, fraction);
        color.y = Lerp(screenFade.fadeg / 255.0f, 1.0f, fraction);
        color.z = Lerp(screenFade.fadeb / 255.0f, 1.0f, fraction);
        color.w = 1.0f;
    }
    else
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        color.x = screenFade.fader / 255.0f;
        color.y = screenFade.fadeg / 255.0f;
        color.z = screenFade.fadeb / 255.0f;
        color.w = alpha / 255.0f;
    }

    {
        glUseProgram(s_shader.program);
        glUniform4fv(s_shader.u_color, 1, &color.x);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

}
