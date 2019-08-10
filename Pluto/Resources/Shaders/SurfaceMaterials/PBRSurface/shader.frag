#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/FragmentVaryings.hxx"
#include "Pluto/Resources/Shaders/FragmentCommon.hxx"
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

void main() {
	EntityStruct entity = ebo.entities[push.consts.target_id];
	MaterialStruct material = mbo.materials[entity.material_id];
	// EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    // CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    // TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];

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
	Contribution contribution = get_ray_traced_contribution(info);
	outColor = vec4(contribution.diffuse_radiance + contribution.specular_radiance, contribution.alpha);
	
	vec2 a = (s_position.xy / s_position.w);
    vec2 b = (s_position_prev.xy / s_position_prev.w);
	// vec2 a = (s_position.xy / s_position.w) * vec2(0.5, -0.5) + 0.5;
    // vec2 b = (s_position_prev.xy / s_position_prev.w) * vec2(0.5, -0.5) + 0.5;
	// a *= vec2(push.consts.width, push.consts.height);
	// b *= vec2(push.consts.width, push.consts.height);
    GBUF1 = vec4(b - a, 0.0, 1.0);

	// vec3 ndc1 = s_position.xyz / s_position.w; //perspective divide/normalize
	// vec3 ndc2 = s_position_prev.xyz / s_position_prev.w; //perspective divide/normalize
	// vec2 viewportCoord1 = ndc1.xy * 0.5 + 0.5; //ndc is -1 to 1 in GL. scale for 0 to 1
	// vec2 viewportCoord2 = ndc2.xy * 0.5 + 0.5; //ndc is -1 to 1 in GL. scale for 0 to 1
	// vec2 viewportPixelCoord1 = viewportCoord1 * vec2(push.consts.width, push.consts.height);
	// vec2 viewportPixelCoord2 = viewportCoord2 * vec2(push.consts.width, push.consts.height);
	// GBUF1 = vec4(viewportPixelCoord2 - viewportPixelCoord1, 0.0, 1.0);
	// Handle emission here...	
	// outColor = vec4(push.consts.target_id + 1, fragTexCoord.x, fragTexCoord.y, 1.0);
	// outPosition = vec4(m_position, 1);
	// outNormal = vec4(N, 1);
	// #else
	// outColor = get_color_contribution(material, N, V, w_position, m_position, fragTexCoord, vert_color);
	// #endif
}
