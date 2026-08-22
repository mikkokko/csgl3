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

// shadow slot uniform data so that subsequent
// commandSetUniformData calls with the same data can be skipped
constexpr int MaxShadowedUniformSize = 64;

struct UniformSlot
{
    int shadowSize; // 0 if not shadowed
    uint8_t shadow[MaxShadowedUniformSize];
};

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
    int baseVertex{};

    GLuint textureUnit{ ~0u };
    GLuint texture2Ds[MaxTextureUnits]{};
    GLuint textureCubeMaps[MaxTextureUnits]{};

    BaseShader *shader{};

    UniformSlot uniformSlots[MaxUniformBlocks]{};
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

// shader programs
void commandUseProgram(BaseShader *shader);

// buffer bindings, vertex attributes and vertex buffer set together for convenience (latched state)
void commandBindVertexBuffer(GLuint buffer, const VertexFormat &format, int baseVertex);
void commandBindIndexBuffer(GLuint buffer);

// does the data upload too for shadowing
void commandSetUniformData(UniformBlock index, const void *data, int size);

// ActiveTexture and BindTexture combined for your convenience
void commandBindTexture(GLuint unit, GLenum target, GLuint texture);

// the the draw calls
void commandDrawElements(GLenum mode, GLsizei count, GLenum type, GLsizei offset);

}

#endif // COMMANDBUFFER_H
