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

	MaterialStruct material = mbo.materials[entity.material_id];


    vec3 barys = vec3(1.0f - bary.x - bary.y, bary.x, bary.y);

	const vec3 A = vec3(1.0, 0.0, 0.0);
	const vec3 B = vec3(0.0, 1.0, 0.0);
	const vec3 C = vec3(0.0, 0.0, 1.0);
	vec3 hit_color = A * barys.x + B * barys.y + C * barys.z;
	payload.color = vec4(material.base_color.rgb, 0.0);//vec4(hit_color.r, hit_color.g, hit_color.b, 0.0);
}