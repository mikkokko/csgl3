#include "stdafx.h"
#include "platform.h"

#if defined(_WIN32)

// workaround for sdks
#define HSPRITE HANDLE_SPRITE
#include <windows.h>

#if !defined(_M_IX86)
#error
#endif

namespace Render
{

typedef void *(*GetDecalTexture_t)(int index);

static Vector3 *s_origin, *s_forward, *s_right, *s_up;
static void **s_currententity;

static GetDecalTexture_t s_getDecalTexture;

static Lightstyle *s_lightstyle;
constexpr Lightstyle s_nullstyle[MAX_LIGHTSTYLES]{}; // dummy for comparisons

static void GetViewVariablePointers(void *pfnGetViewInfo)
{
    uint8_t *data = reinterpret_cast<uint8_t *>(pfnGetViewInfo);

    // 25t
    const uint8_t patternNew[] = { 0x55, 0x8B, 0xEC, 0xF3, 0x0F, 0x10, 0x05 };
    if (!memcmp(data, patternNew, sizeof(patternNew)))
    {
        s_origin = *(Vector3 **)(data + 7);
        s_up = *(Vector3 **)(data + 48);
        s_right = *(Vector3 **)(data + 89);
        s_forward = *(Vector3 **)(data + 133);
        return;
    }

    // pre 25
    const uint8_t pattern[] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08 };
    if (!memcmp(data, pattern, sizeof(pattern)))
    {
        s_origin = *(Vector3 **)(data + 8);
        s_up = *(Vector3 **)(data + 31);
        s_right = *(Vector3 **)(data + 60);
        s_forward = *(Vector3 **)(data + 89);
        return;
    }

    // classical
    const uint8_t patternOld[] = { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x0D };
    if (!memcmp(data, patternOld, sizeof(patternOld)))
    {
        s_origin = *(Vector3 **)(data + 6);
        s_up = *(Vector3 **)(data + 29);
        s_right = *(Vector3 **)(data + 59);
        s_forward = *(Vector3 **)(data + 89);
        return;
    }

    platformError("Could not get pointers to vpn, vright, vup");
}

static void GetCurrentEntityPointer(void *pfnGetCurrentEntity)
{
    uint8_t *instruction = (uint8_t *)pfnGetCurrentEntity;
    if (*instruction == 0xA1) // mov eax, currententity
    {
        s_currententity = *(void ***)(instruction + 1);
        return;
    }

    platformError("Could not get a pointer to currententity");
}

constexpr uint16_t Mask = 0xffff;

static bool PatternMatches(const uint8_t *haystack, const uint16_t *needle, int needleSize)
{
    for (int i = 0; i < needleSize; i++)
    {
        if (needle[i] == Mask)
        {
            continue;
        }

        if (haystack[i] != needle[i])
        {
            return false;
        }
    }

    return true;
}

static void *FindCodePattern(uint8_t *start, uint8_t *end, const uint16_t *pattern, int patternSize)
{
    void *match = nullptr;

    for (uint8_t *address = start; address < end - patternSize; address++)
    {
        if (PatternMatches(address, pattern, patternSize))
        {
            if (match)
            {
                // matched twice?
                return nullptr;
            }

            match = address;
        }
    }

    return match;
}

// Attention ! For blobbed engines this is going to reutnr
static bool GetSection(void *pointerInSection, uint8_t **pstart, uint8_t **pend)
{
    HMODULE module;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<LPCSTR>(pointerInSection),
            &module))
    {
        return false;
    }

    uint8_t *base = reinterpret_cast<uint8_t *>(module);

    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dosHeader->e_lfanew);

    DWORD relative = static_cast<DWORD>(static_cast<uint8_t *>(pointerInSection) - base);

    PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++)
    {
        DWORD sectionStart = sectionHeader[i].VirtualAddress;
        DWORD sectionSize = sectionHeader[i].Misc.VirtualSize;
        DWORD sectionEnd = sectionStart + sectionSize;

        if (relative >= sectionStart && relative < sectionEnd)
        {
            *pstart = base + sectionStart;
            *pend = *pstart + sectionSize;
            return true;
        }
    }

    return false;
}

static void FindGetDecalTexture(void *pointerInCodeSection)
{
    uint8_t *start, *end;
    if (!GetSection(pointerInCodeSection, &start, &end))
    {
        platformError("Failed to get engine code section bounds");
    }

    // 25
    const uint16_t patternNew[] = { 0x55, 0x8B, 0xEC, 0x8B, 0x4D, 0x08, 0x85, 0xC9, 0x79 };
    s_getDecalTexture = (GetDecalTexture_t)FindCodePattern(start, end, patternNew, Q_countof(patternNew));
    if (s_getDecalTexture)
    {
        return;
    }

    // pre 25
    const uint16_t pattern[] = { 0x55, 0x8B, 0xEC, 0x8B, 0x4D, 0x08, 0x56, 0x85 };
    s_getDecalTexture = (GetDecalTexture_t)FindCodePattern(start, end, pattern, Q_countof(pattern));
    if (s_getDecalTexture)
    {
        return;
    }

    // classical
    const uint16_t patternOld[] = { 0x8B, Mask, 0x24, 0x04, 0x56, 0x85, Mask, 0x57, 0x7D };
    s_getDecalTexture = (GetDecalTexture_t)FindCodePattern(start, end, patternOld, Q_countof(patternOld));
    if (s_getDecalTexture)
    {
        return;
    }

    platformError("Could not get a pointer to Draw_DecalTexture");
}

static void FindLightstyle(void *pointerInCodeSection, void *pointerInDataSection)
{
    // a 100% unreliable way to get a pointer to cl_lightstyle
    uint8_t *codeStart, *codeEnd;
    if (!GetSection(pointerInCodeSection, &codeStart, &codeEnd))
    {
        platformError("Failed to get engine code section bounds");
    }

    // wtf -  works with all official windows and linux builds
    uint16_t pattern[] = { 0x83, 0xE8, 0x61 };

    uint8_t *address = (uint8_t *)FindCodePattern(codeStart, codeEnd, pattern, Q_countof(pattern));
    if (!address)
    {
        platformError("Failed to find R_AnimateLight");
    }

    uint8_t *start, *end;
    if (!GetSection(pointerInDataSection, &start, &end))
    {
        platformError("Failed to get engine data section bounds");
    }

    // max 64 bytes back as a safeguard
    for (int i = 0; i < 64; i++)
    {
        // read the potential address
        uint8_t *test = *(uint8_t **)address;

        // make sure it's at least 4-aligned, and check the bounds
        if (((uintptr_t)test % 4) == 0 && start < test && end > test)
        {
            // this is most likely it, if it's not 16-aligned, bring it back 4 bytes
            if (((uintptr_t)test % 16) != 0)
            {
                test -= 4;
            }

            // it hasn't been written to yet so it must be all zeroes
            if (!memcmp(test, &s_nullstyle, sizeof(s_nullstyle)))
            {
                s_lightstyle = (Lightstyle *)test;
                return;
            }
        }

        address--;
    }

    platformError("Could not get a pointer to cl_lightstyle");
}

void platformInit(void *pfnGetViewInfo, void *pfnGetCurrentEntity, void *viewEntity)
{
    GetViewVariablePointers(pfnGetViewInfo);
    GetCurrentEntityPointer(pfnGetCurrentEntity);
    FindGetDecalTexture(pfnGetCurrentEntity);
    FindLightstyle(pfnGetCurrentEntity, viewEntity);
}

[[noreturn]] void platformError(const char *format, ...)
{
    va_list ap;
    char buffer[4096];

    va_start(ap, format);
    Q_vsprintf(buffer, format, ap);
    va_end(ap);

    MessageBoxA(NULL, buffer, PRODUCT_NAME, MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

void platformSetViewInfo(
    const Vector3 &origin,
    const Vector3 &forward,
    const Vector3 &right,
    const Vector3 &up)
{
    GL3_ASSERT(s_origin && s_forward && s_right && s_up);
    *s_origin = origin;
    *s_forward = forward;
    *s_right = right;
    *s_up = up;
}

void platformSetCurrentEntity(void *entity)
{
    GL3_ASSERT(s_currententity);
    *s_currententity = entity;
}

void *platformGetDecalTexture(int index)
{
    GL3_ASSERT(s_getDecalTexture);
    return s_getDecalTexture(index);
}

const Lightstyle &platformLightstyleString(int style)
{
    GL3_ASSERT(style >= 0 && style < MAX_LIGHTSTYLES);
    return s_lightstyle[style];
}

}

#endif
