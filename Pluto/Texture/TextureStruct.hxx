/* File shared by both GLSL and C++ */

#if APPLE || __APPLE__
// On Mac, a maximum of 128 concurrent textures can be in use.
// Since we have tex2D, 3D, and cubemaps, we can do 40 textures
// per texture array.
#ifndef MAX_TEXTURES
#define MAX_TEXTURES 40
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
