/* File shared by both GLSL and C++ */

#if __APPLE__
#ifndef MAX_TEXTURES
#define MAX_TEXTURES 16
#endif
#else
#ifndef MAX_TEXTURES
#define MAX_TEXTURES 64
#endif
#endif

#ifndef MAX_SAMPLERS
#define MAX_SAMPLERS 16
#endif

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

#ifdef GLSL
#define int32_t int
#endif

struct TextureStruct
{
    vec4 color1;
    vec4 color2;
    float scale;
    int32_t mip_levels;
    int32_t sampler_id;
    int32_t type;
};
