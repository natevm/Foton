
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

/* This struct must stay under 128 bytes (32 items 4 bytes each) 

    Limit currently imposed by Radeon RX 560
*/

struct PushConsts
{
    int32_t target_id;
    int32_t camera_id;
    int32_t brdf_lut_id;
    int32_t ltc_mat_lut_id;
    int32_t ltc_amp_lut_id;
    int32_t environment_id;
    int32_t diffuse_environment_id;
    int32_t specular_environment_id;
    int32_t viewIndex;
    int32_t light_entity_ids [MAX_LIGHTS];
    int32_t ph1;
    int32_t ph2;
    
    float sky_transition;
    float gamma;
    float exposure;
    float time;
    float environment_roughness;

    vec4 top_sky_color;
    vec4 bottom_sky_color;
};

#endif
