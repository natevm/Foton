/* File shared by both GLSL and C++ */

#ifndef MAX_TEXTURES
//#define MAX_TEXTURES 64 // Limit imposed by intel. Can be much larger on descrete GPUs
#define MAX_TEXTURES 16 // Limit imposed by stupid macOS. Can be much larger on computers with real GPUs and drivers
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
    float ph1;
    float ph2;
    int32_t type;
};
