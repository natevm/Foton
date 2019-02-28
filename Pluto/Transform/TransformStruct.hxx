/* File shared by both GLSL and C++ */

#ifndef MAX_TRANSFORMS
#define MAX_TRANSFORMS 2048
#endif

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

struct TransformStruct
{
    mat4 worldToLocal;
    mat4 localToWorld;
    mat4 worldToLocalRotation;
    mat4 localToWorldRotation;
    mat4 worldToLocalTranslation;
    mat4 localToWorldTranslation;
    mat4 worldToLocalScale;
    mat4 localToWorldScale;
};
