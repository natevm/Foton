/* File shared by both GLSL and C++ */

#ifndef MAX_MULTIVIEW
#define MAX_MULTIVIEW 6
#endif

#ifndef MAX_CAMERAS
#define MAX_CAMERAS 32
#endif

#ifndef GLSL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_FORCE_RIGHT_HANDED
#include <glm/glm.hpp>
using namespace glm;
#endif

/* This could be split up to allow for better GPU memory reads */
struct CameraObject
{
    mat4 view;
    mat4 proj;
    mat4 viewinv;
    mat4 projinv;
    mat4 viewproj;
    float near_pos;
    // float far_pos;
    float fov;
    float pad2;
    int tex_id;
};

struct CameraStruct
{
    CameraObject multiviews[MAX_MULTIVIEW];
};