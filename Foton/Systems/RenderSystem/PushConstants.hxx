
// File compiled by both c++ and glsl
#ifndef PUSHCONSTANTS_HXX
#define PUSHCONSTANTS_HXX

#include "Foton/Light/LightStruct.hxx"

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
    // int32_t sobel_tile_id;
    // int32_t ranking_tile_id;
    // int32_t scramble_tile_id;
    int32_t frame;
    int32_t width; // Might be able to repurpose this, as launch size is sometimes available
    int32_t height; // Might be able to repurpose this, as launch size is sometimes available
    int32_t flags;
    int32_t num_lights;
    int32_t iteration;
    
    float sky_transition;
    float gamma;
    float exposure;
    float time;
    float environment_roughness;
    float environment_intensity;
    float parameter1;
    float parameter2;
    float parameter3;

    vec4 top_sky_color;
    vec4 bottom_sky_color;
};

#endif
