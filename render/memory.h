#ifndef MEMORY_H
#define MEMORY_H

namespace Render
{

void memoryInit();

void *memoryStaticAlloc(int size, int alignment);

void *memoryLevelAlloc(int size, int alignment);
void memoryLevelFree();

void *memoryTempAlloc(int size, int alignment);
void memoryTempFree(int count);

template<typename T>
T *memoryStaticAlloc(int count)
{
    T *result = static_cast<T *>(memoryStaticAlloc(count * sizeof(T), alignof(T)));
    memset(result, 0, count * sizeof(T));
    return result;
}

template<typename T>
T *memoryLevelAlloc(int count)
{
    T *result = static_cast<T *>(memoryLevelAlloc(count * sizeof(T), alignof(T)));
    memset(result, 0, count * sizeof(T));
    return result;
}

class TempMemoryScope
{
public:
    TempMemoryScope() = default;
    TempMemoryScope(const TempMemoryScope &) = delete;
    TempMemoryScope(TempMemoryScope &&) = delete;

    ~TempMemoryScope()
    {
        memoryTempFree(m_count);
    }

    template<typename T>
    T *Alloc(int count)
    {
        if (!count)
        {
            GL3_ASSERT(false);
            return nullptr;
        }

        m_count++;

        void *result = memoryTempAlloc(count * sizeof(T), alignof(T));
        memset(result, 0, sizeof(T) * count);
        return static_cast<T *>(result);
    }

private:
    int m_count{};
};

}

#endif //MEMORY_H
