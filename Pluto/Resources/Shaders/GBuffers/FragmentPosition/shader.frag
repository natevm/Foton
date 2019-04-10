#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"

layout(location = 0) in vec4 w_position;
layout(location = 0) out vec4 outFragColor;

void main() 
{
    outFragColor = vec4(normalize(w_position.xyz), 1.0);
}