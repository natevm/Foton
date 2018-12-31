#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable

#define GLSL
#include "Pluto/Entity/EntityStruct.hxx"
#include "Pluto/Material/MaterialStruct.hxx"
#include "Pluto/Light/LightStruct.hxx"
#include "Pluto/Transform/TransformStruct.hxx"
#include "Pluto/Camera/CameraStruct.hxx"
#include "Pluto/Texture/TextureStruct.hxx"

#define MAX_MULTIVIEW 6
layout(std430, set = 0, binding = 0) readonly buffer EntitySSBO    { EntityStruct entities[]; } ebo;
layout(std430, set = 0, binding = 1) readonly buffer TransformSSBO { TransformStruct transforms[]; } tbo;
layout(std430, set = 0, binding = 2) readonly buffer CameraSSBO    { CameraStruct cameras[]; } cbo;
layout(std430, set = 0, binding = 3) readonly buffer MaterialSSBO  { MaterialStruct materials[]; } mbo;
layout(std430, set = 0, binding = 4) readonly buffer LightSSBO     { LightStruct lights[]; } lbo;

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