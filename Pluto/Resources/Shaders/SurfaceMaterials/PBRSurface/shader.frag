#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/FragmentVaryings.hxx"
#include "Pluto/Resources/Shaders/FragmentCommon.hxx"
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

void main() {
	EntityStruct entity = ebo.entities[push.consts.target_id];
	MaterialStruct material = mbo.materials[entity.material_id];

	vec3 N = perturbNormal(material, m_position, normalize(m_normal), fragTexCoord, m_position);
	if (!gl_FrontFacing) {
		N *= -1.0;
	}
	vec3 V = normalize(w_cameraPos - w_position);
	
	// Handle emission here...	
	outColor = vec4(push.consts.target_id + 1, fragTexCoord.x, fragTexCoord.y, 1.0);
	outPosition = vec4(m_position, 1);
	outNormal = vec4(N, 1);
	// #else
	// outColor = get_color_contribution(material, N, V, w_position, m_position, fragTexCoord, vert_color);
	// #endif
}
