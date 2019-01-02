
// File compiled by both c++ and glsl
#ifndef PUSHCONSTANTS_HXX
#define PUSHCONSTANTS_HXX

#include "Pluto/Light/LightStruct.hxx"

struct PushConsts {
	int target_id;
    int camera_id;
    int brdf_lut_id;
    int environment_id;
    int diffuse_environment_id;
    int specular_environment_id;
    float gamma;
    float exposure;
    int light_entity_ids [MAX_LIGHTS];
};

#endif