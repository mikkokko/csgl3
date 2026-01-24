cmake_minimum_required(VERSION 3.13)

project(render)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

if (MSVC)
    add_compile_options(/MP)
    add_compile_options(/W4 /wd4100)
    add_compile_options(/arch:SSE2 /fp:fast /GS-)

    #add_compile_options(/Zi)
    #add_link_options(/DEBUG)

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-m32)
    add_link_options(-m32)

    add_compile_options(-Wall -Wextra -Wno-write-strings -Wno-unused-parameter -Wno-invalid-offsetof)
    add_compile_options(-march=x86-64 -fpic)

    add_compile_options("$<$<CONFIG:Release>:-Ofast>")
endif()

if (WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0501)
endif()

option(MESHOPT_INSTALL "Install library" OFF)
add_subdirectory(external/meshoptimizer)

add_executable(shaderc shaderc/main.cpp)
target_include_directories(shaderc PRIVATE external/stb)

set(RENDER_SRC
    render/beam.cpp
    render/brush.cpp
    render/commandbuffer.cpp
    render/decal.cpp
    render/dynamicbuffer.cpp
    render/effects.cpp
    render/entity.cpp
    render/gamma.cpp
    render/hudgl3.cpp
    render/immediate.cpp
    render/internal_goldsrc.cpp
    render/lightmap.cpp
    render/lightstyle.cpp
    render/linmath.cpp
    render/loader.cpp
    render/loader.cpp
    render/memory.cpp
    render/particle.cpp
    render/platform_linux.cpp
    render/platform_windows.cpp
    render/pvs.cpp
    render/stdafx.cpp
    render/random.cpp
    render/render.cpp
    render/screenfadegl3.cpp
    render/shader.cpp
    render/skybox.cpp
    render/sprite.cpp
    render/studio_cache.cpp
    render/studio_misc.cpp
    render/studio_proxy.cpp
    render/studio_render.cpp
    render/texture.cpp
    render/triapigl3.cpp
    render/water.cpp)

if (MSVC)
    set_source_files_properties(${RENDER_SRC} PROPERTIES COMPILE_FLAGS "/Yustdafx.h")
    set_source_files_properties(render/stdafx.cpp PROPERTIES COMPILE_FLAGS "/Ycstdafx.h")
endif()

# add glad and build it as c++
list(APPEND RENDER_SRC external/glad/src/glad.c)
set_source_files_properties(external/glad/src/glad.c PROPERTIES LANGUAGE CXX)

add_library(render SHARED ${RENDER_SRC})
set_target_properties(render PROPERTIES PREFIX "")

target_include_directories(render PRIVATE external/stb external/glad/include external/sdk/common external/sdk/engine external/sdk/pm_shared external/sdk/public)

target_link_libraries(render PRIVATE meshoptimizer)

# shader junk
set(SHADER_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
file(GLOB SHADER_SOURCES ${SHADER_DIR}/*.vert ${SHADER_DIR}/*.frag)
set(SHADER_SOURCES_FILE ${CMAKE_CURRENT_BINARY_DIR}/shader_sources.inl)

target_compile_definitions(render PRIVATE
    SHADER_PATH="${SHADER_DIR}"
    SHADER_SOURCES_FILE="${SHADER_SOURCES_FILE}")

add_custom_command(
    OUTPUT ${SHADER_SOURCES_FILE}
    COMMAND shaderc ${SHADER_SOURCES} > ${SHADER_SOURCES_FILE}
    DEPENDS shaderc ${SHADER_SOURCES}
    VERBATIM)

target_sources(render PRIVATE ${SHADER_SOURCES_FILE})

if (OUTDIR)
    add_custom_command(TARGET render POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:render>
        ${OUTDIR}/$<TARGET_FILE_NAME:render>)
endif()
