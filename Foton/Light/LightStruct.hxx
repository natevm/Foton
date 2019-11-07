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
	float intensity_cutoff; 
	float radius;
	int softnessSamples;
	// 100 - double sided
	// 010 - show end caps
	// 001 - cast shadows
	int flags;
};

#ifdef GLSL
float get_light_radius(inout LightStruct light_struct)
#else
inline float get_light_radius(LightStruct &light_struct)
#endif
{
	return light_struct.radius * (sqrt(light_struct.intensity / light_struct.intensity_cutoff) - 1.0f);
}


#ifdef GLSL
float get_light_attenuation(inout LightStruct light_struct, float distance)
#else
inline float get_light_attenuation(LightStruct &light_struct, float distance)
#endif
{
	float attenuation = 1.0f / pow(((distance/light_struct.radius) + 1.f), 2);
	attenuation = (attenuation - light_struct.intensity_cutoff) / (1.0f - light_struct.intensity_cutoff);
	#ifndef GLSL
	attenuation = std::max(attenuation, 0.0f);
	#else
	attenuation = max(attenuation, 0.0f);
	#endif

	// #ifndef GLSL
	// attenuation *= glm::clamp( (light_struct.radius - distance) / light_struct.radius , 0.0f, 1.0f);
	// #else
	// attenuation *= clamp( (light_struct.radius - distance) / light_struct.radius , 0.0f, 1.0f);
	// #endif

	return attenuation;
}

#endif