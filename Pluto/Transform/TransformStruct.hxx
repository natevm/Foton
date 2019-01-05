/* File shared by both GLSL and C++ */

#ifndef MAX_TRANSFORMS
#define MAX_TRANSFORMS 256
#endif

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

struct TransformStruct
{
    mat4 worldToLocal;
    mat4 localToWorld;
};