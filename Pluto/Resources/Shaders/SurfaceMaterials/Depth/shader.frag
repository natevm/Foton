#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform CameraBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewinv;
    mat4 projinv;
    float near;
    float far;
} cbo;

layout(binding = 1) uniform UniformBufferObject {
  mat4 modelMatrix;
  /* Since this depth texture may come from a different projection, 
  	we have near and far as part of the UBO */
  float near;
  float far;
} ubo;

layout(binding = 2) uniform sampler2D depthSampler;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

float LinearizeDepth(float depth)
{
  float n = ubo.near; // camera z near
  float f = ubo.far; // camera z far
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}

void main() 
{
	float depth = texture(depthSampler, fragTexCoord).r;
	outColor = vec4(vec3(1.0-LinearizeDepth(depth)), 1.0);
}