#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 fragColor;

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
	mat4 normalMatrix;
} ubo;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    gl_Position = cbo.proj * cbo.view * ubo.modelMatrix * vec4(inPosition, 1.0);
    fragColor = vec4(normalize(inNormal), 1.0f);
}