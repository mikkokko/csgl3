// shader combiner: reads all shader sources, expands their includes and writes
// them as c arrays to stdout (which gets redirected to shader_sources.inl by cmake)
#include <cstring>
#include <iomanip>
#include <iostream>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL
#include "stb_include.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

static std::string file_dir(const char *path)
{
    const char *start1 = strrchr(path, '/');
    const char *start2 = strrchr(path, '\\');

    const char *start;
    if (start1 && start2)
    {
        start = (start1 > start2) ? start1 : start2;
    }
    else
    {
        start = start1 ? start1 : (start2 ? start2 : path);
    }

    return { path, start };
}

static std::string file_name(const char *path)
{
    const char *start1 = strrchr(path, '/');
    const char *start2 = strrchr(path, '\\');

    const char *start;
    if (start1 && start2)
    {
        start = (start1 > start2) ? start1 + 1 : start2 + 1;
    }
    else
    {
        start = start1 ? start1 + 1 : (start2 ? start2 + 1 : path);
    }

    return start;
}

static std::string pretty_name(const char *path)
{
    const char *start1 = strrchr(path, '/');
    const char *start2 = strrchr(path, '\\');

    const char *start;
    if (start1 && start2)
    {
        start = (start1 > start2) ? start1 + 1 : start2 + 1;
    }
    else
    {
        start = start1 ? start1 + 1 : (start2 ? start2 + 1 : path);
    }

    std::string result{ start };

    auto pos = result.rfind('.');
    if (pos != std::string::npos)
    {
        result[pos] = '_';
    }

    return result;
}

int main(int argc, char **argv)
{
    std::cout << "// automatically generated\n";

    for (int i = 1; i < argc; i++)
    {
        char *path = argv[i];

        char error[256];
        char *data = stb_include_file(path, nullptr, const_cast<char *>(file_dir(path).c_str()), error);
        if (!data)
        {
            std::cerr << error;
            return 1;
        }

        std::string pretty = pretty_name(path);
        std::cout << "static const unsigned char s_" << pretty << "[] =\n{\n";

        size_t length = std::strlen(data);
        for (size_t j = 0; j < length; j++)
        {
            if (j && (j < length - 1) && (j % 16) == 0)
            {
                std::cout << "\n";
            }

            std::cout << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (data[j] & 0xff) << ",";
        }

        std::cout << "\n};\n";

        free(data);
    }

    std::cout << "static const ShaderData s_shaderData[] =\n{\n";

    for (int i = 1; i < argc; i++)
    {
        const char *path = argv[i];
        std::string pretty = pretty_name(path);
        std::cout << "{\"" << file_name(path) << "\",s_" << pretty << ",sizeof(s_" << pretty << ")},\n";
    }

    std::cout << "};\n";

    return 0;
}
