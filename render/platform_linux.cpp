#include "stdafx.h"
#include "platform.h"

#if defined(__linux__)

#include <dlfcn.h>
#include <link.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#if !defined(__i386__)
#error
#endif

namespace Render
{

typedef void *(*GetDecalTexture_t)(int index);

static Vector3 *s_origin, *s_forward, *s_right, *s_up;
static void **s_currententity;

static GetDecalTexture_t s_getDecalTexture;

static Lightstyle *s_lightstyle;

// urgh what the fuck!!! we are manually parsing symtab
// FIXME: no way to get these from memory? do we really need to map the file ourselves?
using Symtab = std::vector<std::pair<std::string, void *>>;

static void GetSymtab(const void *addressInModule, Symtab &symbols)
{
    Dl_info info;
    if (!dladdr(addressInModule, &info))
    {
        platformError("Could not get engine load address");
    }

    int fd = open(info.dli_fname, O_RDONLY);
    if (fd < 0)
    {
        platformError("Could not open file %s", info.dli_fname);
    }

    off_t fileSize = lseek(fd, 0, SEEK_END);
    void *fileBase = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (fileBase == MAP_FAILED)
    {
        platformError("Could not map file %s", info.dli_fname);
    }

    Elf32_Ehdr *header = (Elf32_Ehdr *)fileBase;
    Elf32_Shdr *sheaders = (Elf32_Shdr *)((uint8_t *)fileBase + header->e_shoff);

    Elf32_Sym *symtab = nullptr;
    size_t symbolCount = 0;
    char *strtab = nullptr;

    for (int i = 0; i < header->e_shnum; i++)
    {
        const Elf32_Shdr &sheader = sheaders[i];
        if (sheader.sh_type == SHT_SYMTAB)
        {
            symtab = (Elf32_Sym *)((uint8_t *)fileBase + sheader.sh_offset);
            symbolCount = sheader.sh_size / sizeof(Elf32_Sym);
            strtab = (char *)((uint8_t *)fileBase + sheaders[sheader.sh_link].sh_offset);
            break;
        }
    }

    if (!symtab)
    {
        platformError("No symtab for %s", info.dli_fname);
    }

    symbols.reserve(symbolCount);

    for (size_t i = 0; i < symbolCount; i++)
    {
        const char *name = &strtab[symtab[i].st_name];
        void *address = (uint8_t *)info.dli_fbase + symtab[i].st_value;
        symbols.emplace_back(name, address);
    }

    munmap(fileBase, fileSize);
}

static void *SymbolAddress(const Symtab &symbols, const char *name)
{
    for (auto &symbol : symbols)
    {
        if (symbol.first == name)
        {
            return symbol.second;
        }
    }

    platformError("Could not find symbol %s", name);
}

static void GetViewVariablePointers(const Symtab &symbols)
{
    s_origin = (Vector3 *)SymbolAddress(symbols, "r_origin");
    s_forward = (Vector3 *)SymbolAddress(symbols, "vpn");
    s_right = (Vector3 *)SymbolAddress(symbols, "vright");
    s_up = (Vector3 *)SymbolAddress(symbols, "vup");
}

static void GetCurrentEntityPointer(const Symtab &symbols)
{
    s_currententity = (void **)SymbolAddress(symbols, "currententity");
}

static void FindGetDecalTexture(const Symtab &symbols)
{
    s_getDecalTexture = (GetDecalTexture_t)SymbolAddress(symbols, "Draw_DecalTexture");
}

static void FindLightstyle(const Symtab &symbols)
{
    s_lightstyle = (Lightstyle *)SymbolAddress(symbols, "cl_lightstyle");
}

void platformInit(void *pfnGetViewInfo, void *, void *)
{
    Symtab symbols;
    GetSymtab(symbols, pfnGetViewInfo);

    GetViewVariablePointers(symbols);
    GetCurrentEntityPointer(symbols);
    FindGetDecalTexture(symbols);
    FindLightstyle(symbols);
}

[[noreturn]] void platformError(const char *format, ...)
{
    va_list ap;
    char buffer[4096];

    va_start(ap, format);
    Q_vsprintf(buffer, format, ap);
    va_end(ap);

    fprintf(stderr, PRODUCT_NAME ": %s\n", buffer);

    // display a message box if we can
    // i don't want to link to sdl2 directly nor do i want it as a build dependency
    void *libSDL2 = dlopen("libSDL2-2.0.so.0", RTLD_LAZY | RTLD_NOLOAD);
    if (libSDL2)
    {
        typedef int (*SDL_ShowSimpleMessageBox_t)(uint32_t flags, const char *title, const char *message, void *window);
        SDL_ShowSimpleMessageBox_t SDL_ShowSimpleMessageBox = (SDL_ShowSimpleMessageBox_t)dlsym(libSDL2, "SDL_ShowSimpleMessageBox");

        if (SDL_ShowSimpleMessageBox)
        {
            SDL_ShowSimpleMessageBox(0, PRODUCT_NAME, buffer, NULL);
        }

        dlclose(libSDL2);
    }

    exit(1);
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
