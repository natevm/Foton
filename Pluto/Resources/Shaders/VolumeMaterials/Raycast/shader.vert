#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 ray_destination;

layout(binding = 0) uniform PerspectiveBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewinv;
    mat4 projinv;
    float near;
    float far;
} pbo;

layout(binding = 1) uniform TransformBufferObject{
    mat4 worldToLocal;
    mat4 localToWorld;
} tbo;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = pbo.proj * pbo.view * tbo.localToWorld * vec4(inPosition, 1.0);
   	ray_destination = inPosition;
}