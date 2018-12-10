#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 worldPositionGeom;
layout(location = 1) out vec3 normalGeom;
layout(location = 2) out vec2 texCoordGeom;
/* Todo: how to handle tex coord */

layout(binding = 0) uniform PBO {
    mat4 view;
    mat4 proj;
    mat4 viewinv;
    mat4 projinv;
    float near;
    float far;
} pbo;

layout(binding = 1) uniform TBO {
    mat4 worldToLocal;
    mat4 localToWorld;
} tbo;

void main(){
	worldPositionGeom = vec3(tbo.localToWorld * vec4(inPosition, 1));
	normalGeom = normalize(mat3(transpose(inverse(tbo.localToWorld))) * inNormal);
	gl_Position = pbo.proj * pbo.view * vec4(worldPositionGeom, 1);
	texCoordGeom = inTexCoord;
}