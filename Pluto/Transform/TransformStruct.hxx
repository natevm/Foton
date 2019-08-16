/* File shared by both GLSL and C++ */

#ifndef MAX_TRANSFORMS
#ifndef LARGE_SCENE_SUPPORT
#define MAX_TRANSFORMS 512
#else
#define MAX_TRANSFORMS 1024
#endif
#endif

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

/* This could be split up to allow for better GPU memory reads */
struct TransformStruct
{
    /* 64 bytes */
    mat4 worldToLocal;
    mat4 localToWorld;
    mat4 worldToLocalRotation;
    mat4 worldToLocalTranslation;

    /* 128 bytes, for temporal reprojection */
    mat4 worldToLocalPrev;
    mat4 localToWorldPrev;
    mat4 worldToLocalRotationPrev;
    mat4 worldToLocalTranslationPrev;


    
    // mat4 localToWorldRotation;
    // mat4 localToWorldTranslation;
    // mat4 worldToLocalScale;
    // mat4 localToWorldScale;
};
