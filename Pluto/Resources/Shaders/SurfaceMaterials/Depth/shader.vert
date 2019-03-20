#version 450
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

layout(location = 0) in vec3 point;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texcoord;

layout(location = 0) out vec3 w_position;
layout(location = 1) out vec3 c_position;

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

    w_position = (target_transform.localToWorld * vec4(point.xyz, 1.0)).xyz;
    c_position = (camera_transform.localToWorld[3]).xyz;

    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (push.consts.use_multiview == 0) ? push.consts.viewIndex : gl_ViewIndex;
    #endif
    gl_Position = camera.multiviews[viewIndex].viewproj * camera_transform.worldToLocalRotation * camera_transform.worldToLocalTranslation * vec4(w_position, 1.0);
}
