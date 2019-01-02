#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable

#include "Pluto/Resources/Shaders/DescriptorLayout.hxx"

layout(location = 0) in vec3 point;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texcoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    EntityStruct entity = ebo.entities[push.consts.target_id];
    CameraStruct camera = cbo.cameras[push.consts.camera_id];
    MaterialStruct material = mbo.materials[entity.material_id];
    TransformStruct transform = tbo.transforms[entity.transform_id];
    
    gl_Position =  camera.multiviews[gl_ViewIndex].proj * camera.multiviews[gl_ViewIndex].view * transform.localToWorld * vec4(point, 1.0);
    gl_PointSize = 1.0;
    fragColor = material.base_color;
    fragTexCoord = texcoord;
}