#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "Pluto/Resources/Shaders/DescriptorLayout.hxx"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in float depth;
layout(location = 2) in float near;

layout(location = 0) out vec4 outColor;

float LinearizeDepth(float depth, float near, float far)
{
  float n = near; // camera z near
  float f = far; // camera z far
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}

vec3 hsv2rgb(vec3 c) {
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

/* Since this project uses zero to one depth (more accurate), I don't think
  I need to linearize the depth... */
void main() 
{
  // might try 1.0 - depth, might also try accounting for near or far, or using a transfer function to determine actual values
	outColor = vec4(vec3(depth), 1.0);
}