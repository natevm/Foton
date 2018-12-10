#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 normal;
layout(location = 1) out vec3 position;
layout(location = 2) out vec3 w_position;
layout(location = 3) out vec2 fragTexCoord;

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
  vec4 kd;
  vec4 ks;
} ubo;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    gl_Position = cbo.proj * cbo.view * ubo.modelMatrix * vec4(inPosition, 1.0);
    normal = vec3(ubo.normalMatrix * vec4(inNormal, 0));

    /* Eye space position */
    position = vec3(cbo.view * ubo.modelMatrix * vec4(inPosition, 1.0));

    /* Compute world space position */
    w_position = vec3(ubo.modelMatrix * vec4(inPosition, 1.0));

    fragTexCoord = inTexCoord;
}