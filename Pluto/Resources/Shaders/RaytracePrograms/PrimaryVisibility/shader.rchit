#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#define RAYTRACING


#include "Pluto/Resources/Shaders/Descriptors.hxx"

layout(location = 0) rayPayloadInNV HitInfo payload;

#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

hitAttributeNV vec2 bary;

void main() {
	payload.entity_id = -1;
	
	/* Invalid Entity Check */
	if (gl_InstanceID < 0 || gl_InstanceID >= MAX_ENTITIES) return;
	EntityStruct entity = ebo.entities[gl_InstanceID];
	if (entity.mesh_id < 0 || entity.mesh_id >= MAX_MESHES) return;
	if (entity.transform_id < 0 || entity.transform_id >= MAX_TRANSFORMS) return;
	if (entity.material_id < 0 || entity.material_id >= MAX_MATERIALS) return;

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
