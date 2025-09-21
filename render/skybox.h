#ifndef SKYBOX_H
#define SKYBOX_H

namespace Render
{

void skyboxInit();

// called every frame, if the skybox texture has changed, loads the new one
void skyboxUpdate(const char (&skyboxName)[32]);

// called by the brush renderer before drawing the sky faces
// now it should only bind the texture and pipeline state
bool skyboxDrawBegin();
void skyboxDrawEnd();

}

#endif // SKYBOX_H
