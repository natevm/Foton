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
    /* The last computed average of all mesh positions */
    glm::vec3 centroid;
    
    /* The radius of a sphere centered at the centroid which contains the mesh */
    float bounding_sphere_radius;

    /* Minimum and maximum bounding box coordinates */
    glm::vec3 bbmin;
    glm::vec3 bbmax;
};
