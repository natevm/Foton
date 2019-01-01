#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable

#include "Pluto/Resources/Shaders/DescriptorLayout.hxx"

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

//layout(binding = 3) uniform sampler2D colorTex;

void main() {
	// if (mbo.useTexture) {
	// 	outColor = texture(colorTex, fragTexCoord);
	// }
	// else {
    outColor = fragColor;
	// }
}