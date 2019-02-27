#ifndef LIGHTSTRUCT_HXX
#define LIGHTSTRUCT_HXX

#ifndef MAX_LIGHTS
#define MAX_LIGHTS 8
#endif

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

// Should support different light types including
//  Directional Lights, which mimic the sun
//  Point Lights, which illuminate in all directions
//  Spot Lights, which have a cone of influence
//  LTC Line lights
//  LTC Rectangle lights 
//  LTC Disk lights 

struct LightStruct {
	vec4 ambient;
	vec4 color;
	float intensity;
	float coneAngle, coneSoftness;
	float constantAttenuation, linearAttenuation, quadraticAttenuation;
	int type; 
	// 100 - double sided
	// 010 - show end caps
	// 001 - cast shadows
	int flags;
};

#endif