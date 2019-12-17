#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#define CAMERA_RAYTRACING


#include "Foton/Resources/Shaders/Common/Descriptors.hxx"

layout(location = 0) rayPayloadInNV HitInfo payload;

#include "Foton/Resources/Shaders/Common/ShaderCommon.hxx"

hitAttributeNV vec2 bary;

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
	bool is_flat = ((dot(N0, N1) > .99) && (dot(N1, N2) > .99));

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

	float edge_curvature_1 = abs(dot((N2-N1), (P2.xyz-P1.xyz))/(distance(P2.xyz,P1.xyz)*distance(P2.xyz,P1.xyz)));
	float edge_curvature_2 = abs(dot((N1-N0), (P1.xyz-P0.xyz))/(distance(P1.xyz,P0.xyz)*distance(P1.xyz,P0.xyz)));
	float edge_curvature_3 = abs(dot((N0-N2), (P0.xyz-P2.xyz))/(distance(P0.xyz,P2.xyz)*distance(P0.xyz,P2.xyz)));

	float v_c_1 = (edge_curvature_2 + edge_curvature_3) * .5;
	float v_c_2 = (edge_curvature_1 + edge_curvature_2) * .5;
	float v_c_3 = (edge_curvature_1 + edge_curvature_3) * .5;
	payload.curvature = barycentrics.x * v_c_1 + barycentrics.y * v_c_2 + barycentrics.z * v_c_3;


	payload.tri = gl_PrimitiveID;
    payload.mesh = entity.mesh_id;
    payload.barycentrics = bary;
}
