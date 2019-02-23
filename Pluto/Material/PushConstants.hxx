
// File compiled by both c++ and glsl
#ifndef PUSHCONSTANTS_HXX
#define PUSHCONSTANTS_HXX

#include "Pluto/Light/LightStruct.hxx"

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

#ifdef GLSL
#define int32_t int
#endif

struct PushConsts
{
    vec4 top_sky_color;
    vec4 bottom_sky_color;
    float sky_transition;
    float gamma;
    float exposure;
    float time;
    float environment_roughness;
    float ph1;
    float ph2;
    float ph3;
    int32_t target_id;
    int32_t camera_id;
    int32_t brdf_lut_id;
    int32_t ltc_mat_lut_id;
    int32_t ltc_amp_lut_id;
    int32_t environment_id;
    int32_t diffuse_environment_id;
    int32_t specular_environment_id;
    int32_t viewIndex;
    int32_t ph11;
    int32_t ph12;
    int32_t ph13;
    int32_t ph14;
    int32_t ph15;
    int32_t ph16;
    int32_t ph17;
    int32_t light_entity_ids[MAX_LIGHTS];
};

#endif
