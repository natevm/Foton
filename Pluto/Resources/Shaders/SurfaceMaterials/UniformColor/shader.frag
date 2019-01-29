#version 450
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

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