#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#define RAYTRACING


#include "Pluto/Resources/Shaders/Common/Descriptors.hxx"

layout(location = 0) rayPayloadInNV HitInfo payload;

#include "Pluto/Resources/Shaders/Common/ShaderCommon.hxx"

hitAttributeNV vec2 bary;

// PBRInfo info;
// int bounce_count = payload.bounce_count;
// info.bounce_count = bounce_count;
// info.entity_id = gl_InstanceID;
// info.w_incoming = vec4(gl_WorldRayDirectionNV, 0.0);
// info.m_position = P;
// info.m_normal = N;
// info.uv = UV;
// Contribution contribution = get_ray_traced_contribution(info);

void main() {
	payload.entity_id = -1;
	
	/* Invalid Entity Check */
	if (gl_InstanceID < 0 || gl_InstanceID >= max_entities) return;
	EntityStruct entity = ebo.entities[gl_InstanceID];
	if (entity.mesh_id < 0 || entity.mesh_id >= max_meshes) return;
	if (entity.transform_id < 0 || entity.transform_id >= max_transforms) return;
	if (entity.material_id < 0 || entity.material_id >= max_materials) return;

	/* Shadow ray */
	if (payload.is_shadow_ray)
	{
		payload.distance = gl_RayTmaxNV;
		payload.entity_id = gl_InstanceID;
		return;
	}

	/* Shaded Ray */
    
	/* Interpolate Vertex Data */
	vec3 barycentrics = vec3(1.0f - bary.x - bary.y, bary.x, bary.y);
	int I0 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 0];
	int I1 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 1];
	int I2 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 2];

	vec3 N0 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I0].xyz;
	vec3 N1 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I1].xyz;
	vec3 N2 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I2].xyz;
	vec3 N = N0 * barycentrics.x + N1 * barycentrics.y + N2 * barycentrics.z;

	// if (dot(N.xyz, -gl_ObjectRayDirectionNV) < 0) N *= -1;

	/* Project surface point out a bit to account for smooth surface normals */
	vec4 P0 = PositionBuffers[nonuniformEXT(entity.mesh_id)].positions[I0];
	vec4 P1 = PositionBuffers[nonuniformEXT(entity.mesh_id)].positions[I1];
	vec4 P2 = PositionBuffers[nonuniformEXT(entity.mesh_id)].positions[I2];
	vec4 P = P0 * barycentrics.x + P1 * barycentrics.y + P2 * barycentrics.z;
	vec3 R0 = P0.xyz + dot(P0.xyz - P.xyz, N0.xyz) * N0.xyz;
	vec3 R1 = P1.xyz + dot(P1.xyz - P.xyz, N1.xyz) * N1.xyz;
	vec3 R2 = P2.xyz + dot(P2.xyz - P.xyz, N2.xyz) * N2.xyz;
	// P = vec4(R0 * barycentrics.x + R1 * barycentrics.y + R2 * barycentrics.z, 1.0);

	// vec4 C0 = ColorBuffers[nonuniformEXT(entity.mesh_id)].colors[I0];
	// vec4 C1 = ColorBuffers[nonuniformEXT(entity.mesh_id)].colors[I1];
	// vec4 C2 = ColorBuffers[nonuniformEXT(entity.mesh_id)].colors[I2];
	// vec4 C = C0 * barycentrics.x + C1 * barycentrics.y + C2 * barycentrics.z;

	vec2 UV0 = TexCoordBuffers[nonuniformEXT(entity.mesh_id)].texcoords[I0].xy;
	vec2 UV1 = TexCoordBuffers[nonuniformEXT(entity.mesh_id)].texcoords[I1].xy;
	vec2 UV2 = TexCoordBuffers[nonuniformEXT(entity.mesh_id)].texcoords[I2].xy;
	vec2 UV = UV0 * barycentrics.x + UV1 * barycentrics.y + UV2 * barycentrics.z;

	/* Save hit data to payload */
	payload.uv = UV;
	// payload.C = C;
	payload.m_n = N;
	payload.m_p = P.xyz;
	payload.w_n = normalize(transpose(mat3(gl_WorldToObjectNV)) * N);
	payload.w_p = (gl_ObjectToWorldNV * P).xyz;
	payload.entity_id = gl_InstanceID;
	payload.distance = gl_RayTmaxNV;
	payload.backface = dot(N, -gl_ObjectRayDirectionNV) < 0.0;
}

// void main() {
// 	payload.entity_id = -1;
// 	payload.contribution.diffuse_radiance = vec3(0.0);
// 	payload.contribution.specular_radiance = vec3(0.0);
// 	payload.contribution.alpha = 0;
	
// 	/* Invalid Entity Check */
// 	if (gl_InstanceID < 0 || gl_InstanceID >= max_entities) return;
// 	EntityStruct entity = ebo.entities[gl_InstanceID];
// 	if (entity.mesh_id < 0 || entity.mesh_id >= max_meshes) return;
// 	if (entity.transform_id < 0 || entity.transform_id >= max_transforms) return;
// 	if (entity.material_id < 0 || entity.material_id >= max_materials) return;

// 	/* Shadow ray */
// 	if (payload.is_shadow_ray)
// 	{
// 		payload.distance = gl_RayTmaxNV;
// 		payload.entity_id = gl_InstanceID;
// 		return;
// 	}

// 	/* Shaded Ray */
//     MaterialStruct material = mbo.materials[entity.material_id];
// 	TransformStruct transform = tbo.transforms[entity.transform_id];

// 	/* Interpolate Vertex Data */
// 	vec3 barycentrics = vec3(1.0f - bary.x - bary.y, bary.x, bary.y);
// 	int I0 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 0];
// 	int I1 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 1];
// 	int I2 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 2];

// 	vec4 N0 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I0];
// 	vec4 N1 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I1];
// 	vec4 N2 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I2];
// 	vec4 N = N0 * barycentrics.x + N1 * barycentrics.y + N2 * barycentrics.z;

// 	if (dot(N.xyz, -gl_ObjectRayDirectionNV) < 0) N *= -1;

// 	/* Project surface point out a bit to account for smooth surface normals */
// 	vec4 P0 = PositionBuffers[nonuniformEXT(entity.mesh_id)].positions[I0];
// 	vec4 P1 = PositionBuffers[nonuniformEXT(entity.mesh_id)].positions[I1];
// 	vec4 P2 = PositionBuffers[nonuniformEXT(entity.mesh_id)].positions[I2];
// 	vec4 P = P0 * barycentrics.x + P1 * barycentrics.y + P2 * barycentrics.z;
// 	vec3 R0 = P0.xyz + dot(P0.xyz - P.xyz, N0.xyz) * N0.xyz;
// 	vec3 R1 = P1.xyz + dot(P1.xyz - P.xyz, N1.xyz) * N1.xyz;
// 	vec3 R2 = P2.xyz + dot(P2.xyz - P.xyz, N2.xyz) * N2.xyz;
// 	P = vec4(R0 * barycentrics.x + R1 * barycentrics.y + R2 * barycentrics.z, 1.0);

// 	vec4 C0 = ColorBuffers[nonuniformEXT(entity.mesh_id)].colors[I0];
// 	vec4 C1 = ColorBuffers[nonuniformEXT(entity.mesh_id)].colors[I1];
// 	vec4 C2 = ColorBuffers[nonuniformEXT(entity.mesh_id)].colors[I2];
// 	vec4 C = C0 * barycentrics.x + C1 * barycentrics.y + C2 * barycentrics.z;

// 	vec2 UV0 = TexCoordBuffers[nonuniformEXT(entity.mesh_id)].texcoords[I0].xy;
// 	vec2 UV1 = TexCoordBuffers[nonuniformEXT(entity.mesh_id)].texcoords[I1].xy;
// 	vec2 UV2 = TexCoordBuffers[nonuniformEXT(entity.mesh_id)].texcoords[I2].xy;
// 	vec2 UV = UV0 * barycentrics.x + UV1 * barycentrics.y + UV2 * barycentrics.z;

// 	vec3 X = vec3(0.), Y = vec3(0.);
//     directionOfAnisotropicity(N.xyz, X, Y);

// 	/* Save the current bounce count */
// 	int bounce_count = payload.bounce_count;

// 	/* Shade this hit point */
// 	SurfaceInteraction interaction;
//     interaction.bounce_count = payload.bounce_count;
//     interaction.entity_id = gl_InstanceID;
//     interaction.w_incoming = vec4(normalize(gl_WorldRayDirectionNV), 0.0);
//     interaction.m_position = P;
//     interaction.m_normal = N;
// 	interaction.m_tangent = vec4(X, 0.0);
// 	interaction.m_binormal = vec4(Y, 0.0);
// 	interaction.random_dimension = payload.random_dimension;//float(push.consts.frame) +// + hash12( payload.pixel );
// 	interaction.w_position = transform.localToWorld * interaction.m_position;
// 	mat4 normal_matrix = /*transpose*/(transform.worldToLocal);
// 	interaction.w_normal = vec4(normalize(normal_matrix * vec4(interaction.m_normal.xyz, 0.0)).xyz, 0.0);
// 	interaction.w_tangent = vec4(normalize(normal_matrix * vec4(interaction.m_tangent.xyz, 0.0)).xyz, 0.0);
// 	interaction.w_binormal = vec4(normalize(normal_matrix * vec4(interaction.m_binormal.xyz, 0.0)).xyz, 0.0);
//     interaction.uv = UV;
// 	interaction.pixel_coords = ivec2(payload.pixel);
// 	interaction.is_specular = payload.is_specular_ray;

// 	vec3 wi; vec3 beta;
// 	Contribution contribution = get_disney_ray_traced_contribution(material, interaction, wi, beta);

// 	// /* Composite alpha if this is a primary visibility ray, up to a limit */
// 	// int hit = 0;
// 	// vec3 origin = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_RayTmaxNV;
// 	// while ((bounce_count == 0) && (contribution.alpha < 1.0) && (hit < 10)) {
// 	// 	uint rayFlags = gl_RayFlagsNoneNV;
// 	// 	uint cullMask = 0xff;
// 	// 	float tmin = .01;
// 	// 	float tmax = 1000.0;
// 	// 	payload.beta = vec3(1.0);
// 	// 	payload.bounce_count = bounce_count + 1;
// 	// 	payload.is_shadow_ray = false;
// 	// 	payload.pixel = gl_LaunchIDNV.xy;
// 	// 	payload.contribution.diffuse_radiance = vec3(0);
//     //     payload.contribution.specular_radiance = vec3(0);
//     //     payload.contribution.alpha = 0;
// 	// 	traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, gl_WorldRayDirectionNV, tmax, 0);

// 	// 	// Not sure how to separate diffuse from specular radiance here....
// 	// 	contribution.diffuse_radiance = (contribution.alpha * contribution.diffuse_radiance) + (payload.contribution.diffuse_radiance + payload.contribution.specular_radiance) * (1.0 - contribution.alpha);
// 	// 	contribution.alpha = contribution.alpha + payload.contribution.alpha * (1.0 - contribution.alpha);

// 	// 	/* Background hit */
// 	// 	if (payload.entity_id == -1) break;
		
// 	// 	/* Move the origin forward */ 
// 	// 	origin = origin + gl_WorldRayDirectionNV * payload.distance;
// 	// 	hit++;
// 	// }

// 	/* Save contribution to payload */
// 	payload.contribution = contribution;
// 	payload.UV = UV;
// 	payload.C = C;
// 	payload.N = N;
// 	payload.P = P;
// 	payload.WN = interaction.w_normal;
// 	payload.WP = interaction.w_position;
// 	payload.entity_id = gl_InstanceID;
// 	payload.distance = gl_RayTmaxNV;
// 	payload.bounce_count = bounce_count;
// 	payload.random_dimension = contribution.random_dimension;
// }