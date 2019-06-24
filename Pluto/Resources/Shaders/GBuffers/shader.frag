#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"

layout(location = 0) in vec4 w_position;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outPosition;

void main() 
{
    // TODO 
    outAlbedo = vec4(normalize(w_position.xyz), 1.0);
    outNormal = vec4(normalize(w_position.xyz), 1.0);
    outPosition = vec4(normalize(w_position.xyz), 1.0);
}