#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable

#include "Pluto/Resources/Shaders/DescriptorLayout.hxx"

layout (location = 0) in vec3 inUVW;
layout (location = 1) in vec3 camPos;

layout (location = 0) out vec4 outFragColor;

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main() 
{
	vec3 color = vec3(0.00, 0.00, 0.00);

	vec3 dir = normalize(inUVW - camPos);
	vec3 adjusted = vec3(dir.x, dir.z, dir.y);
	
	if (push.consts.environment_id != -1) {
		color = texture(
			samplerCube(texture_cubes[push.consts.environment_id], samplers[push.consts.environment_id]), 
			adjusted
		).rgb;
	}

	// Tone mapping
	color = Uncharted2Tonemap(color * push.consts.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

	// Gamma correction
	color = pow(color, vec3(1.0f / push.consts.gamma));

	outFragColor = vec4(color, 1.0);
}