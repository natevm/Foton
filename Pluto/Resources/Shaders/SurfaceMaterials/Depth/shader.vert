#version 450
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

layout(location = 0) in vec3 point;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texcoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out float depth;
layout(location = 2) out float near;

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
    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = gl_ViewIndex;
    #endif
    gl_Position = camera.multiviews[viewIndex].proj * camera.multiviews[viewIndex].view * camera_transform.worldToLocal * w_position;
    fragTexCoord = texcoord;
    near = camera.multiviews[viewIndex].near_pos;
    depth = (gl_Position.z - near);
}
