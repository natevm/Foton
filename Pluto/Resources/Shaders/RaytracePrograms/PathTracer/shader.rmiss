#version 460
#extension GL_NV_ray_tracing : require
#define RAYTRACING

#include "Pluto/Resources/Shaders/Descriptors.hxx"

layout(location = 0) rayPayloadInNV HitInfo payload;

#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

void main() {
    payload.m_n = vec3(0.);
    payload.w_n = vec3(0.);
    payload.m_p = vec3(0.);
    payload.w_p = vec3(0.);
    payload.uv = vec2(0.);
    payload.entity_id = -1;
    payload.distance = -1;
}