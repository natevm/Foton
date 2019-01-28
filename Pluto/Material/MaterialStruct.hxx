/* File shared by both GLSL and C++ */
#ifndef MAX_MATERIALS
#define MAX_MATERIALS 256
#endif

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

#ifdef GLSL
#define int32_t int
#endif

/* Not all these properties are mapped to PBR. */
struct MaterialStruct {
    vec4 base_color; // 16
    vec4 subsurface_radius; // 32
    vec4 subsurface_color; //48
    float subsurface; // 52
    float metallic; // 56
    float specular; // 60
    float specular_tint; // 64
    float roughness; // 68
    float anisotropic; // 72
    float anisotropic_rotation; // 76
    float sheen; // 80
    float sheen_tint; // 84
    float clearcoat; // 88
    float clearcoat_roughness; // 92
    float ior; // 96
    float transmission; // 100
    float transmission_roughness; // 104
    float ph4; // 108
    float ph5; // 112
    int32_t flags; // 116
    int32_t base_color_texture_id; // 120
    int32_t roughness_texture_id; // 124
    int32_t occlusion_texture_id; // 128
};
