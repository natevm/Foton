#version 460
#extension GL_NV_ray_tracing : require
#define RAYTRACING

#include "Pluto/Resources/Shaders/Descriptors.hxx"

layout(location = 0) rayPayloadInNV HitInfo payload;

void main() {
    //payload.color += vec4(0.412f, 0.796f, 1.0f, 1.0f);
    payload.color += vec4(0.0f, 0.0f, .0f, 0.0f);
}
