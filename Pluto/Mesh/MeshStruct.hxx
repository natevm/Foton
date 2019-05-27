/* File shared by both GLSL and C++ */
#ifndef MAX_MESHES
#ifndef LARGE_SCENE_SUPPORT
#define MAX_MESHES 1024
#else
#define MAX_MESHES 2048
#endif
#endif

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

#ifdef GLSL
#define int32_t int
#endif

/* Not all these properties are mapped to PBR. */
struct MeshStruct {
    mat4 bb_local_to_parent;

    /* The last computed average of all mesh positions */
    vec4 center; // 16
    
    /* Minimum and maximum bounding box coordinates */
    vec4 bbmin; // 32
    vec4 bbmax; // 48
    
    /* The radius of a sphere centered at the centroid which contains the mesh */
    float bounding_sphere_radius; // 52
    int32_t show_bounding_box; // 56
    int32_t ph2; // 60
    int32_t ph3; // 64

};
