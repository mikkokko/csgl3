#ifndef LIGHTSTYLE_H
#define LIGHTSTYLE_H

namespace Render
{

struct LightmapSamples;

extern float g_lightstyles[MAX_LIGHTSTYLES];

void lightstyleUpdate();
void lightstyleReset();

Vector3 lightstyleApply(const LightmapSamples &samples);

}

#endif
