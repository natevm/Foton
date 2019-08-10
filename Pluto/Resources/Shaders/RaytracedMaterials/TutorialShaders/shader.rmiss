#version 460
#extension GL_NV_ray_tracing : require
#define RAYTRACING

#include "Pluto/Resources/Shaders/Descriptors.hxx"

layout(location = 0) rayPayloadInNV HitInfo payload;

#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

void main() {
    payload.N = vec4(0.);
    payload.P = vec4(0.);
    payload.C = vec4(0.);
    payload.UV = vec2(0.);
    payload.entity_id = -1;
    payload.distance = -1;
    payload.contribution.diffuse_radiance = get_environment_color(gl_WorldRayDirectionNV);
    payload.contribution.specular_radiance = vec3(0.0);
    payload.contribution.alpha = 1.0;
}
