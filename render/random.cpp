#include "stdafx.h"
#include "random.h"

namespace Render
{

// static initalization abuse
static uint32_t s_state = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&s_state));

static uint32_t Next()
{
    uint32_t x = s_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_state = x;

    return x;
}

int randomInt(int min, int max)
{
    GL3_ASSERT(min <= max);
    uint32_t range = max - min + 1;
    uint32_t value = Next() % range;
    return min + value;
}

float randomFloat(float min, float max)
{
    GL3_ASSERT(min <= max);
    float frac = static_cast<float>(Next() >> 8) * 0x1.0p-24f;
    return min + (max - min) * frac;
}

}
