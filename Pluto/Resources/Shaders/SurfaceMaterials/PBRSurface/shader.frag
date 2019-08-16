#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/FragmentVaryings.hxx"
#include "Pluto/Resources/Shaders/FragmentCommon.hxx"
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

void main() 
{
	/* These are required for shading purposes. Wait until they're ready. */
    if ((push.consts.ltc_mat_lut_id < 0) || (push.consts.ltc_mat_lut_id >= MAX_TEXTURES)) return;
    if ((push.consts.ltc_amp_lut_id < 0) || (push.consts.ltc_amp_lut_id >= MAX_TEXTURES)) return;
    randomDimension = 0;

	/* Compute ray origin and direction */
	const ivec2 pixel_coords = ivec2(gl_FragCoord.xy);
	const vec2 pixel_center = vec2(gl_FragCoord.xy) + vec2(random(pixel_coords), random(pixel_coords));
	const vec2 in_uv = pixel_center / vec2(push.consts.width, push.consts.height);
	vec2 d = in_uv * 2.0 - 1.0; d.y *= -1.0;

	EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];

	vec3 origin = (camera_transform.localToWorld * camera.multiviews[push.consts.viewIndex].viewinv * vec4(0,0,0,1)).xyz;
	vec3 target = (camera.multiviews[push.consts.viewIndex].projinv * vec4(d.x, d.y, 1, 1)).xyz ;
    vec3 direction = (camera_transform.localToWorld * camera.multiviews[push.consts.viewIndex].viewinv * vec4(normalize(target), 0)).xyz ;
    direction = normalize(direction);

	/* Rasterizing... Just calculate first hit. */
	vec4 color = vec4(0.0);
	// bool last_event_was_specular = true;
	// vec3 beta = vec3(1.0);
 	// int last_direct_light_entity_id = -2;
	
	/* We hit an object. Load any additional required data. */ 
	EntityStruct entity; TransformStruct transform; 
	MaterialStruct mat; LightStruct light;
	unpack_entity(push.consts.target_id, entity, transform, mat, light);

	// EntityStruct hit_entity = ebo.entities[push.consts.target_id];
	// TransformStruct hit_entity_transform = tbo.transforms[hit_entity.transform_id];
	// MaterialStruct hit_entity_material = mbo.materials[hit_entity.material_id];
	// LightStruct hit_entity_light;
	// bool hit_entity_is_emissive = ((hit_entity.light_id < MAX_LIGHTS) && (hit_entity.light_id >= 0));
	// if (hit_entity_is_emissive) hit_entity_light = lbo.lights[hit_entity.light_id];
	// mat3 normal_matrix = transpose(mat3(hit_entity_transform.worldToLocal));

	// /* Setup surface interaction */
	// SurfaceInteraction SI;
	// SI.entity_id = push.consts.target_id;
	// SI.m_p = vec4(m_position, 1.0);
	// SI.m_n = m_normal;
	// // SI.m_t = payload.X;
	// // SI.m_b = payload.Y;
	// SI.w_i = direction;
	// SI.w_p = hit_entity_transform.localToWorld * SI.m_p;
	// SI.w_n = normalize(normal_matrix * SI.m_n);
	// directionOfAnisotropicity(SI.w_n, SI.w_t, SI.w_b);
	// SI.uv = fragTexCoord;
	// SI.is_specular = false;
	// SI.pixel_coords = pixel_coords;
	// vec3 Ld = vec3(0.);
	// vec3 wi;
    // vec3 wo = -SI.w_i.xyz;

	// // for now, flip normals to face the camera
	// if (dot(SI.w_n, direction) > 0) {
	// 	SI.w_n *= -1.0; SI.w_t *= -1.0; SI.w_b *= -1.0;
	// 	SI.m_n *= -1.0; SI.m_t *= -1.0; SI.m_b *= -1.0;
	// }

	// /* Compute radiance from the hit object back to source. */

	// /* If the hit object was emissive and the last event was specular, radiate */
	// if (hit_entity_is_emissive && last_event_was_specular) {
	// 	Ld += hit_entity_light.color.rgb * hit_entity_light.intensity;
	// }

	// /* Pick a random light for next event estimation */
	// int light_entity_id;
	// EntityStruct light_entity;
	// TransformStruct light_transform;
	// LightStruct light;
	// bool light_found = false;
	// int tries = 0;
	// while ((!light_found) && (tries < 3) && (push.consts.num_lights > 0)) {
	// 	tries++;
	// 	int i = int((random(SI.pixel_coords) - EPSILON) * (push.consts.num_lights));
	// 	/* Skip unused lights and lights without a transform */
	// 	light_entity_id = lidbo.lightIDs[i];
	// 	if ((light_entity_id < 0) || (light_entity_id >= MAX_ENTITIES)) break;
	// 	// if (light_entity_id == interaction.entity_id) break;
	// 	light_entity = ebo.entities[light_entity_id];
	// 	if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) break;
	// 	light_found = true;
	// 	light = lbo.lights[light_entity.light_id];
	// 	light_transform = tbo.transforms[light_entity.transform_id];
	// }

	// /* If we hit a light */
	// if (light_found) {
	// 	// Importance sample the light directly
    //     /* Compute the outgoing radiance for this light */
    //     float lightPdf = 0., visibility = 1.;
    //     bool light_is_double_sided = bool(light.flags & (1 << 0));
    //     mat3 m_inv;

    //     /* Compute direct specular and diffuse contribution from LTC area lights */
    //     vec2 LTC_UV = vec2(max(hit_entity_material.roughness, EPSILON), sqrt(1.0 - dot(SI.w_n.xyz, -SI.w_i.xyz)))*LUT_SCALE + LUT_BIAS;
    //     vec4 t1 = texture(sampler2D(texture_2Ds[push.consts.ltc_mat_lut_id], samplers[0]), LTC_UV);
    //     vec4 t2 = texture(sampler2D(texture_2Ds[push.consts.ltc_amp_lut_id], samplers[0]), LTC_UV);
    //     m_inv = mat3(
    //         vec3(t1.x, 0, t1.y),
    //         vec3(  0,  1,    0),
    //         vec3(t1.z, 0, t1.w)
    //     );

	// 	LTCContribution contribution = rectangleLightSampleLTC(light_entity, light, light_transform, m_inv, SI);
	// 	// lightPdf = contribution.lightPdf;
	// 	// wi = contribution.wi;
	// 	// Li = contribution.differential_irradiance;
	// 	// Ld += contribution.diffuse_irradiance + contribution.specular_irradiance;

	// 	/* TODO: Compute illumination using LTC */
	// 	color.rgb += contribution.diffuse_irradiance + contribution.specular_irradiance;
	// }
	

	// // /* Setup origin and direction for next ray */
	// // bool is_specular;
	// // float brdf_weight = 1.0;
	// // float brdf_scattering_pdf = 1.0;
	// // vec3 f = bsdfSample(random(SI.pixel_coords), random(SI.pixel_coords), random(SI.pixel_coords), 
	// // 	wi, wo, brdf_scattering_pdf, is_specular, SI, hit_entity_material);
	// // f *= abs(dot(wi, SI.w_n.xyz));
	// // bool isBlack = dot(f, f) <= EPSILON;
	// // // if (isBlack) break;
	// // beta *= f * (brdf_weight / max(brdf_scattering_pdf, EPSILON));
	// // origin = SI.w_p.xyz;
	// // direction = wi;
	// // last_direct_light_entity_id = light_entity_id;
	// // last_event_was_specular = false;

	// // /* We hit the sky.  */
	// // vec3 irradiance = get_environment_color(direction);
	// // if (!any(isnan(beta * irradiance))) 
	// // 	color.rgb += beta * irradiance;
	
	// /* Write out to G Buffers */
	// // outColor = color;
	GBUF1 = vec4(0);
	outColor = getAlbedo(mat, vert_color, fragTexCoord, m_position);


	// EntityStruct entity = ebo.entities[push.consts.target_id];
	// MaterialStruct material = mbo.materials[entity.material_id];
	// // EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    // // CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    // // TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];

	// vec3 N = perturbNormal(material, m_position, normalize(m_normal), fragTexCoord, m_position);
	// // if (!gl_FrontFacing) {
	// // 	N *= -1.0;
	// // }
	// vec3 V = normalize(w_cameraPos - w_position);
	
	// PBRInfo info;
	// info.bounce_count = 0;
	// info.entity_id = push.consts.target_id;
	// info.w_incoming = vec4(-V, 0.0);
	// info.m_position = vec4(m_position.xyz, 1.0);
	// info.m_normal = vec4(N.xyz, 0.0);
	// info.uv = vec2(fragTexCoord.x, fragTexCoord.y);
	// Contribution contribution;// = get_ray_traced_contribution(info);
	// contribution.diffuse_radiance = N.xyz;
	// contribution.alpha = 1.0;
	// outColor = vec4(contribution.diffuse_radiance + contribution.specular_radiance, contribution.alpha);
	
	// vec2 a = (s_position.xy / s_position.w);
    // vec2 b = (s_position_prev.xy / s_position_prev.w);
	// // vec2 a = (s_position.xy / s_position.w) * vec2(0.5, -0.5) + 0.5;
    // // vec2 b = (s_position_prev.xy / s_position_prev.w) * vec2(0.5, -0.5) + 0.5;
	// // a *= vec2(push.consts.width, push.consts.height);
	// // b *= vec2(push.consts.width, push.consts.height);
    // GBUF1 = vec4(b - a, 0.0, 1.0);

	// // vec3 ndc1 = s_position.xyz / s_position.w; //perspective divide/normalize
	// // vec3 ndc2 = s_position_prev.xyz / s_position_prev.w; //perspective divide/normalize
	// // vec2 viewportCoord1 = ndc1.xy * 0.5 + 0.5; //ndc is -1 to 1 in GL. scale for 0 to 1
	// // vec2 viewportCoord2 = ndc2.xy * 0.5 + 0.5; //ndc is -1 to 1 in GL. scale for 0 to 1
	// // vec2 viewportPixelCoord1 = viewportCoord1 * vec2(push.consts.width, push.consts.height);
	// // vec2 viewportPixelCoord2 = viewportCoord2 * vec2(push.consts.width, push.consts.height);
	// // GBUF1 = vec4(viewportPixelCoord2 - viewportPixelCoord1, 0.0, 1.0);
	// // Handle emission here...	
	// // outColor = vec4(push.consts.target_id + 1, fragTexCoord.x, fragTexCoord.y, 1.0);
	// // outPosition = vec4(m_position, 1);
	// // outNormal = vec4(N, 1);
	// // #else
	// // outColor = get_color_contribution(material, N, V, w_position, m_position, fragTexCoord, vert_color);
	// // #endif
}
