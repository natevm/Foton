#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(binding = 2) uniform MaterialBufferObject {
    vec4 color;
    bool useTexture;
} mbo;

layout (binding = 3) uniform samplerCube samplerCubeMap;

layout (location = 0) in vec3 inUVW;
layout (location = 1) in vec3 camPos;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	if (mbo.useTexture)
		outFragColor = texture(samplerCubeMap, inUVW - camPos);
	else 
		outFragColor = mbo.color;
}