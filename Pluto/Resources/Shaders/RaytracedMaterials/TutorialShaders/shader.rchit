#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#define RAYTRACING

#include "Pluto/Resources/Shaders/Descriptors.hxx"

layout(location = 0) rayPayloadInNV HitInfo payload;
hitAttributeNV vec2 bary;

void main() {
    vec3 barys = vec3(1.0f - bary.x - bary.y, bary.x, bary.y);

	const vec3 A = vec3(1.0, 0.0, 0.0);
	const vec3 B = vec3(0.0, 1.0, 0.0);
	const vec3 C = vec3(0.0, 0.0, 1.0);
	vec3 hit_color = A * barys.x + B * barys.y + C * barys.z;
	payload.color = vec4(1.0, 0.0, 0.0, 0.0);
}