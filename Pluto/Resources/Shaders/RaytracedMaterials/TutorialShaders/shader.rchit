#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#define RAYTRACING

#include "Pluto/Resources/Shaders/Descriptors.hxx"
//#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

layout(location = 0) rayPayloadInNV HitInfo payload;
hitAttributeNV vec2 bary;

void main() {
	if (gl_InstanceID < 0 || gl_InstanceID >= MAX_ENTITIES) {
		payload.entity_id = -1;
		return;
	}
	EntityStruct entity = ebo.entities[gl_InstanceID];
	if (entity.mesh_id < 0 || entity.mesh_id >= MAX_MESHES) return;

	vec3 barycentrics = vec3(1.0f - bary.x - bary.y, bary.x, bary.y);
	int I0 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 0];
	int I1 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 1];
	int I2 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 2];

	vec4 N0 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I0];
	vec4 N1 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I1];
	vec4 N2 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I2];
	payload.N = N0 * barycentrics.x + N1 * barycentrics.y + N2 * barycentrics.z;

	vec4 P0 = PositionBuffers[nonuniformEXT(entity.mesh_id)].positions[I0];
	vec4 P1 = PositionBuffers[nonuniformEXT(entity.mesh_id)].positions[I1];
	vec4 P2 = PositionBuffers[nonuniformEXT(entity.mesh_id)].positions[I2];
	payload.P = P0 * barycentrics.x + P1 * barycentrics.y + P2 * barycentrics.z;

	vec4 C0 = ColorBuffers[nonuniformEXT(entity.mesh_id)].colors[I0];
	vec4 C1 = ColorBuffers[nonuniformEXT(entity.mesh_id)].colors[I1];
	vec4 C2 = ColorBuffers[nonuniformEXT(entity.mesh_id)].colors[I2];
	payload.C = C0 * barycentrics.x + C1 * barycentrics.y + C2 * barycentrics.z;

	vec2 UV0 = TexCoordBuffers[nonuniformEXT(entity.mesh_id)].texcoords[I0].xy;
	vec2 UV1 = TexCoordBuffers[nonuniformEXT(entity.mesh_id)].texcoords[I1].xy;
	vec2 UV2 = TexCoordBuffers[nonuniformEXT(entity.mesh_id)].texcoords[I2].xy;
	payload.UV = UV0 * barycentrics.x + UV1 * barycentrics.y + UV2 * barycentrics.z;

	payload.entity_id = gl_InstanceID;
	payload.distance = gl_RayTmaxNV;
}