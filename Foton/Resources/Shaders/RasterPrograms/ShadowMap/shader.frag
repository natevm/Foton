#version 450
#define RASTER
#define SHADOW_MAP

#include "Foton/Resources/Shaders/Common/Descriptors.hxx"

layout(location = 0) in vec3 w_position;
layout(location = 1) in vec3 c_position;

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
  vec3 distvec = w_position - c_position;
  float len = length(distvec);
  outMoments = vec4(len, len * len, len * len * len, len * len * len * len); // todo: complete moment shadow mapping...
}