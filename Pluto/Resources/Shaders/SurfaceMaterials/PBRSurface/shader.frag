#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/FragmentVaryings.hxx"
#include "Pluto/Resources/Shaders/FragmentCommon.hxx"
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

void main() {
	EntityStruct entity = ebo.entities[push.consts.target_id];
	MaterialStruct material = mbo.materials[entity.material_id];

	vec3 N = perturbNormal(material, m_position, normalize(m_normal), fragTexCoord, m_position);
	// if (!gl_FrontFacing) {
	// 	N *= -1.0;
	// }
	vec3 V = normalize(w_cameraPos - w_position);
	
	PBRInfo info;
	info.bounce_count = 0;
	info.entity_id = push.consts.target_id;
	info.w_incoming = vec4(-V, 0.0);
	info.m_position = vec4(m_position.xyz, 1.0);
	info.m_normal = vec4(N.xyz, 0.0);
	info.uv = vec2(fragTexCoord.x, fragTexCoord.y);
	vec4 color = get_ray_traced_contribution(info);
	vec3 final_color;

	/* Tone mapping */
    final_color = Uncharted2Tonemap(color.rgb * push.consts.exposure);
    final_color = final_color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

    /* Gamma correction */
    final_color = pow(final_color, vec3(1.0f / push.consts.gamma));

	outColor = vec4(final_color, color.a);
	
	// Handle emission here...	
	// outColor = vec4(push.consts.target_id + 1, fragTexCoord.x, fragTexCoord.y, 1.0);
	// outPosition = vec4(m_position, 1);
	// outNormal = vec4(N, 1);
	// #else
	// outColor = get_color_contribution(material, N, V, w_position, m_position, fragTexCoord, vert_color);
	// #endif
}
