#ifndef UTILITY_H
#define UTILITY_H

namespace Render
{

struct Color24
{
    byte r, g, b;
};

struct Color32
{
    byte r, g, b, a;
};

template<typename T>
constexpr const T &Q_min(const T &a, const T &b)
{
    return (a < b) ? a : b;
}

template<typename T>
constexpr const T &Q_max(const T &a, const T &b)
{
    return (a > b) ? a : b;
}

template<typename T>
constexpr const T &Q_clamp(const T &a, const T &b, const T &c)
{
    return (a > c) ? c : ((a < b) ? b : a);
}

template<typename T, int N>
constexpr int Q_countof(const T (&)[N])
{
    return N;
}

// if you don't know the buffer sizes, kill yourself
template<size_t A, size_t B>
inline size_t Q_strcpy(char (&dest)[A], const char (&src)[B])
{
    static_assert(A >= B, "Potential string truncation");
    size_t length = strlen(src);
    memcpy(dest, src, length + 1);
    return length;
}

template<size_t N>
inline bool Q_strcpy_truncate(char (&dest)[N], const char *src)
{
    size_t length = strnlen(src, N);
    if (length == N)
    {
        memcpy(dest, src, N - 1);
        dest[N - 1] = '\0';
        return false;
    }

    memcpy(dest, src, length + 1);
    return true;
}

template<int N>
inline int Q_sprintf(char (&dest)[N], const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int result = vsnprintf(dest, N, fmt, ap);
    GL3_ASSERT(result > 0 && result < N);
    va_end(ap);
    return result;
}

template<int N>
inline int Q_vsprintf(char (&dest)[N], const char *fmt, va_list ap)
{
    return vsnprintf(dest, N, fmt, ap);
}

inline int Q_strcasecmp(const char *s1, const char *s2)
{
#ifdef _MSC_VER
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

// 32-bit FNV-1a
inline uint32_t HashString(const char *string)
{
    uint32_t hash = 0x811c9dc5;

    for (; *string; string++)
    {
        hash ^= static_cast<uint8_t>(*string);
        hash *= 0x01000193;
    }

    return hash;
}

template<typename T>
static T AlignUp(T address, int alignment)
{
    if (!address)
    {
        return {};
    }

    uintptr_t temp = (uintptr_t)address;
    temp += alignment - temp % alignment;
    return (T)temp;
}

}

#endif // UTILITY_H
