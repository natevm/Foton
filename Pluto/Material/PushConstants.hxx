
// File compiled by both c++ and glsl
#ifndef PUSHCONSTANTS_HXX
#define PUSHCONSTANTS_HXX

#include "Pluto/Light/LightStruct.hxx"

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

struct PushConsts {
	int target_id;
    int camera_id;
    int brdf_lut_id;
    int environment_id;
    vec4 top_sky_color;
    vec4 bottom_sky_color;
    float sky_transition;
    int diffuse_environment_id;
    int specular_environment_id;
    float gamma;
    float exposure;
    float time;
    int light_entity_ids [MAX_LIGHTS];
    int ph2;
    int ph3;
    int ph4;
    int ph5;
    int ph6;
    int ph7;
};

#endif