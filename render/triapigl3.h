#ifndef TRIAPIGL3_H
#define TRIAPIGL3_H

namespace Render
{

// original engine triapi
extern triangleapi_t g_triapiGL1;

void triapiInit();

void triapiBegin();
void triapiEnd();

// draw studio model shadows after the models to avoid redundant state changes
void triapiQueueStudioShadow(int sprite, float *p1, float *p2, float *p3, float *p4);

}

#endif
