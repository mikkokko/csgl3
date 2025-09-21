// immediate mode style wrapper, used by triapi and sprites
#ifndef IMMEDIATE_H
#define IMMEDIATE_H

namespace Render
{

void immediateInit();

// all drawing must be guarded with these
void immediateDrawStart(bool alphaTest);
void immediateDrawEnd();

// true if in a Start/End
bool immediateIsActive();

void immediateBlendEnable(GLboolean enable);
void immediateBlendFunc(GLenum sfactor, GLenum dfactor);
void immediateCullFace(GLboolean enable);
void immediateDepthTest(GLboolean enable);
void immediateDepthMask(GLboolean flag);

void immediateBindTexture(GLuint texture);

void immediateBegin(GLenum mode);
void immediateColor4f(float r, float g, float b, float a);
void immediateTexCoord2f(float s, float t);
void immediateVertex3f(float x, float y, float z);
void immediateEnd();

}

#endif
