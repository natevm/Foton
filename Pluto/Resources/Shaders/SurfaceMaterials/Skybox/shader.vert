#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;

#define MAX_MULTIVIEW 6
struct PerspectiveObject {
    mat4 view;
    mat4 proj;
    mat4 viewinv;
    mat4 projinv;
    float near;
    float far;
    float pad1, pad2;
};

layout(binding = 0) uniform PerspectiveBufferObject {
    PerspectiveObject at[MAX_MULTIVIEW];
} pbo;

layout(binding = 1) uniform TransformBufferObject{
    mat4 worldToLocal;
    mat4 localToWorld;
} tbo;

layout(binding = 2) uniform MaterialBufferObject {
    vec4 color;
    bool useTexture;
} mbo;

layout (location = 0) out vec3 outUVW;
layout (location = 1) out vec3 camPos;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main() 
{
	outUVW.x = inPos.x;
	outUVW.y = inPos.z;
	outUVW.z = inPos.y;
	vec3 cameraPos = vec3(tbo.worldToLocal * pbo.viewinv[3]);
	camPos.x = cameraPos.x;
	camPos.y = cameraPos.z;
	camPos.z = cameraPos.y;
	
	gl_Position = pbo.at[gl_ViewIndex].proj * pbo.at[gl_ViewIndex].view * tbo.localToWorld * vec4(inPos.xyz, 1.0);
}
