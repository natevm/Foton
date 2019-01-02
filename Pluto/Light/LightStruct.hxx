#ifndef LIGHTSTRUCT_HXX
#define LIGHTSTRUCT_HXX

#ifndef MAX_LIGHTS
#define MAX_LIGHTS 8
#endif

#ifndef GLSL
#include <glm/glm.hpp>
using namespace glm;
#endif

struct LightStruct {
	vec4 position;
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;

	float ambientContribution, constantAttenuation, linearAttenuation, quadraticAttenuation;
	float spotCutoff, spotExponent, pad1, pad2;
};

#endif