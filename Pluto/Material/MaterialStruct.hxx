/* File shared by both GLSL and C++ */

#ifndef MAX_MATERIALS
#define MAX_MATERIALS 1024
#endif

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

struct MaterialStruct {
    vec4 base_color;
    vec4 subsurface_radius;
    vec4 subsurface_color;
    float subsurface;
    float metallic;
    float specular;
    float specular_tint;
    float roughness;
    float anisotropic;
    float anisotropic_rotation;
    float sheen;
    float sheen_tint;
    float clearcoat;
    float clearcoat_roughness;
    float ior;
    float transmission;
    float transmission_roughness;
    
    int base_color_texture_id;
    int metalic_roughness_texture_id;
    float ph3;
    float ph4;
    float ph5;
    float ph6;
};
