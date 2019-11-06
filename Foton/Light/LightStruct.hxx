#ifndef LIGHTSTRUCT_HXX
#define LIGHTSTRUCT_HXX

#ifndef MAX_LIGHTS
#define MAX_LIGHTS 8
#endif

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

#ifndef LIGHT_FLAGS
#define LIGHT_FLAGS

#define LIGHT_FLAGS_DOUBLE_SIDED (1<<0)
#define LIGHT_FLAGS_SHOW_END_CAPS (1<<1)
#define LIGHT_FLAGS_CAST_SHADOWS (1<<2)
#define LIGHT_FLAGS_USE_VSM (1<<3)
#define LIGHT_FLAGS_DISABLED (1<<4)
#define LIGHT_FLAGS_POINT (1<<5)
#define LIGHT_FLAGS_SPHERE (1<<6)
#define LIGHT_FLAGS_DISK (1<<7)
#define LIGHT_FLAGS_ROD (1<<8)
#define LIGHT_FLAGS_PLANE (1<<9)

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
	float softnessRadius;
	float ph3;
	float ph4; 
	int softnessSamples;
	// 100 - double sided
	// 010 - show end caps
	// 001 - cast shadows
	int flags;
};

#endif