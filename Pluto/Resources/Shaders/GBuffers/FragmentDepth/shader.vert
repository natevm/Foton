#version 450
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"
#include "Pluto/Resources/Shaders/VertexCommon.hxx"

void main() {
    EntityStruct target_entity = ebo.entities[push.consts.target_id];
    EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];
    TransformStruct target_transform = tbo.transforms[target_entity.transform_id];

    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = gl_ViewIndex;
    #endif

    fragTexCoord = texcoord;
    gl_Position = camera.multiviews[viewIndex].proj * camera.multiviews[viewIndex].view * camera_transform.worldToLocal * target_transform.localToWorld * vec4(point.xyz, 1.0);
    gl_Position -= vec4(0.0, 0.0, .0001, 0.0);
}
