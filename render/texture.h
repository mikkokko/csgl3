#ifndef TEXTURE_H
#define TEXTURE_H

namespace Render
{

// this stuff is currently hyper specific to tiled textures
// exists so we can avoid redundant work and update gl_texturemode on them

void textureInit();

// called at the start of every frame
void textureUpdate();

// can't use glCreateTextures because it might get us a name
// that gets later clobbered by the engine (old engines do not use glGenTextures)
void textureGenTextures(GLsizei n, GLuint *textures);

// see if a texture by this name is already loaded, return the opengl name if so
GLuint textureFind(GLenum target, const char *name);

// create a new texture with this name and set the correct texture mode
// note that if the texture might already exist, you should check with Find before calling this
GLuint textureAllocateAndBind(GLenum target, const char *name, bool mipmapped);

// helper to load a texture from a file
GLuint textureLoad2D(const char *path, bool mipmapped, bool gamma);

}

#endif
