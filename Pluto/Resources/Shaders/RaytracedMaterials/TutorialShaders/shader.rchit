#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#define RAYTRACING

#include "Pluto/Resources/Shaders/Descriptors.hxx"

layout(location = 0) rayPayloadInNV HitInfo payload;
hitAttributeNV vec2 bary;

void main() {
	if (gl_InstanceID < 0 || gl_InstanceID > MAX_ENTITIES) return;
	EntityStruct entity = ebo.entities[gl_InstanceID];
	if (entity.material_id < 0 || entity.material_id > MAX_MATERIALS) return;
	if (entity.mesh_id < 0 || entity.mesh_id > MAX_MESHES) return;

	MaterialStruct material = mbo.materials[entity.material_id];

    vec3 barycentrics = vec3(1.0f - bary.x - bary.y, bary.x, bary.y);

	int I0 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 0];
	int I1 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 1];
	int I2 = IndexBuffers[nonuniformEXT(entity.mesh_id)].indices[3 * gl_PrimitiveID + 2];

	vec3 N0 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I0].xyz;
	vec3 N1 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I1].xyz;
	vec3 N2 = NormalBuffers[nonuniformEXT(entity.mesh_id)].normals[I2].xyz;

	vec3 hit_color = N0 * barycentrics.x + N1 * barycentrics.y + N2 * barycentrics.z;
	payload.color = vec4(hit_color.rgb, 0.0);//vec4(hit_color.r, hit_color.g, hit_color.b, 0.0);
}