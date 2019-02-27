#version 450
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"
#include "Pluto/Resources/Shaders/FragmentCommon.hxx"

void main() 
{
	vec3 color = vec3(0.00, 0.00, 0.00);

	vec3 dir = normalize(w_position - w_cameraPos);
	vec3 adjusted = vec3(dir.x, dir.z, dir.y);
	
	color = get_environment_color(adjusted);

	// Tone mapping
	color = Uncharted2Tonemap(color * push.consts.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

	// Gamma correction
	color = pow(color, vec3(1.0f / push.consts.gamma));

	outColor = vec4(color, 1.0);
}
