> [!WARNING]
> Heavily incomplete, not usable for normal play

## Requirements

* OpenGL 3.1 or later
* ARB_draw_elements_base_vertex
* Any Steam version of the game on either Windows or Linux

## Missing features

* Most of the EFX API is not implemented
* Most rendering related cvars do not work with the renderer (e.g. gl_wireframe, r_fullbright)
* Detail textures are not supported
* Studio model texture color remaps are not implemented
* Spectator subviews do not draw properly

## Behavioral differences

The renderer tries to remain faithful to the engine's renderer, but there are some intentional and incidental differences:

* Dlights are applied per-pixel and do not scale with the lightmap
* Dlights are limited to 4 visible at a time
* Studio model lighting is computed per-pixel
* Tiling textures are atlased
* Lightstyles are interpolated
* Water waves have no varying height
* Fog behaviour on translucent entities is different
* Underwater fog uses an exponential-squared falloff rather than linear
* Studio model glowshell effect may look different
* Chrome textures on models are not constrained to 64x64 and may appear different
* Fullbright texture flag on studio models is supported
* Skybox textures are no longer limited to 256x256, but all faces must have the same size
* NVGs will not spawn dlights

## Installation

* Compile and install [Client-side loader from the gl3 branch](https://github.com/mikkokko/csldr/tree/gl3)
* Compile this project
* Move `render.dll` / `render.so` to the `cl_dlls` folder (where `client.dll` / `client.so` resides)
* Launch the game. The cvar gl3_enable should be available
