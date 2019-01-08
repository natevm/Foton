#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable

#include "Pluto/Resources/Shaders/DescriptorLayout.hxx"

layout(location = 0) in vec3 point;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texcoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out float depth;
layout(location = 2) out float near;
layout(location = 3) out float far;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    EntityStruct target_entity = ebo.entities[push.consts.target_id];
    EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];

    TransformStruct target_transform = tbo.transforms[target_entity.transform_id];

    vec4 w_position = target_transform.localToWorld * vec4(point.xyz, 1.0);
    gl_Position = camera.multiviews[gl_ViewIndex].proj * camera.multiviews[gl_ViewIndex].view * camera_transform.worldToLocal * w_position;
    fragTexCoord = texcoord;
    near = camera.multiviews[gl_ViewIndex].near_pos;
    far = camera.multiviews[gl_ViewIndex].far_pos;
    depth = (gl_Position.z - near) / (far - near);
}