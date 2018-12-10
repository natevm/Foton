#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable
precision highp float;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 w_pos;
layout(location = 1) out vec4 w_lightPos;

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
    float pointSize;
    bool useTexture;
} mbo;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    w_pos = tbo.localToWorld * vec4(inPosition, 1.0);
    w_lightPos = pbo.at[gl_ViewIndex].viewinv[3];

    gl_Position = pbo.at[gl_ViewIndex].proj * pbo.at[gl_ViewIndex].view * w_pos;
    gl_PointSize = mbo.pointSize;
}