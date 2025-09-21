#ifndef PLATFORM_H
#define PLATFORM_H

namespace Render
{

// must match the engine counterpart
struct Lightstyle
{
    int size;
    char data[64];
};

// exits the process on failure
void platformInit(void *pfnGetViewInfo, void *pfnGetCurrentEntity, void *viewEntity);

// fatal error, show a message box if possible and exit the process
[[noreturn]] void platformError(const char *format, ...);

void platformSetViewInfo(
    const Vector3 &origin,
    const Vector3 &forward,
    const Vector3 &right,
    const Vector3 &up);

void platformSetCurrentEntity(void *entity);

void *platformGetDecalTexture(int index);

const Lightstyle &platformLightstyleString(int style);

}

#endif
