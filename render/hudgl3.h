#ifndef HUDGL3_H
#define HUDGL3_H

namespace Render
{

void hudInit(cl_enginefunc_t *engfuncs);

bool hudWorldToScreen(const Vector3 &p, float &x, float &y);

}

#endif
