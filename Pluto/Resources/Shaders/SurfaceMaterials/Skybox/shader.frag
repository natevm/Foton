#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/FragmentVaryings.hxx"
#include "Pluto/Resources/Shaders/FragmentCommon.hxx"
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

void main() 
{
	vec3 color = vec3(0.00, 0.00, 0.00);
	vec3 dir = normalize(w_position - w_cameraPos);
	color = get_environment_color(dir);
	outColor = vec4(color, 1.0);
	GBUF1 = vec4(0.0, 0.0, -1.0, 1.0);
}
