#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

// This program's dynamic buffer management was written for ARB_buffer_storage, but after
// the downgrade to GL 3.1 that is no longer an option. I don't want to map/unmap between draw calls, so I've
// added a command buffer layer that does the following so we still have the ergonomics of persistent coherent map:
// - Dynamic buffers are mapped at the start of command buffer recording
// - Commands are recorded to a buffer
// - Buffers are unmapped after recording completes
// - The recorded GL calls are executed

namespace Render
{

constexpr unsigned MaxTextureUnits = 4;

// shadowing state to reduce command buffer sizes, overhead is negligible
// exposed in header for immediate.cpp (FIXME: does it really need this?)
struct ShadowState
{
    GLboolean blendEnable{ GL_FALSE };
    GLenum blendSrc{};
    GLenum blendDst{};

    GLboolean depthTest{ GL_TRUE };
    GLenum depthFunc{};
    GLboolean depthMask{ GL_TRUE };

    GLboolean cullFace{ GL_TRUE };

    GLuint vertexBuffer{};
    GLuint indexBuffer{};
    const VertexFormat *vertexFormat{};

    GLuint textureUnit{ ~0u };
    GLuint texture2Ds[MaxTextureUnits]{};
    GLuint textureCubeMaps[MaxTextureUnits]{};

    BaseShader *shader{};
};

extern ShadowState g_shadowState;

void commandInit();

void commandRecord();
void commandExecute();

// blend state
void commandBlendEnable(GLboolean enable);
void commandBlendFunc(GLenum sfactor, GLenum dfactor);

// depth stencil state
void commandDepthTest(GLboolean enable);
void commandDepthFunc(GLenum func);
void commandDepthMask(GLboolean flag);

// rasterizer state
void commandCullFace(GLboolean enable);
void commandPolygonOffset(GLfloat factor, GLfloat units);

// programs and default uniform block
void commandUseProgram(BaseShader *shader);
void commandUniform1f(GLint location, GLfloat v0);
void commandUniform1i(GLint location, GLint v0);

// buffer bindings, vertex attributes and vertex buffer set together for convenience (latched state)
void commandBindVertexBuffer(GLuint buffer, const VertexFormat &format);
void commandBindIndexBuffer(GLuint buffer);
void commandBindUniformBuffer(GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);

// ActiveTexture and BindTexture combined for your convenience
void commandBindTexture(GLuint unit, GLenum target, GLuint texture);

// the the draw calls
void commandDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, GLsizei offset, GLint basevertex);

}

#endif // COMMANDBUFFER_H
