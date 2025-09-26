// this file is included by both c++ code and shaders

// FIXME: bullshit way of doing dlights!!!
#define MAX_SHADER_LIGHTS 4

// already defined, will cause a warning if the definition doesn't match
#define MAX_LIGHTSTYLES 64

// engine constants, already defined in studio.h
// will cause a warning if the definition doesn't match
#define MAXSTUDIOBONES 128
#define MAX_SHADER_BONES MAXSTUDIOBONES

// used to be shader permutation flags, should remove...
#define STUDIO_SHADER_FLATSHADE (1 << 0) // flatshade texture flag
#define STUDIO_SHADER_CHROME (1 << 1) // chrome texture flag
#define STUDIO_SHADER_FULLBRIGHT (1 << 2) // fullbright texture flag
#define STUDIO_SHADER_COLOR_ONLY (1 << 3) // use the color uniform as-is for tinting, used for additive and glowshell
#define STUDIO_SHADER_ELIGHTS (1 << 4)

// there's probably an engine constant for this...
#define STUDIO_MAX_ELIGHTS 3
