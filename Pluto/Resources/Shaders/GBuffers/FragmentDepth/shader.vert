#version 450
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"
#include "Pluto/Resources/Shaders/VertexCommon.hxx"

void main() {
    EntityStruct target_entity = ebo.entities[push.consts.target_id];
    EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];
    TransformStruct target_transform = tbo.transforms[target_entity.transform_id];

    vec4 w_position;
    // if (push.consts.show_bounding_box > 0) {
    //     w_position = push.consts.bounding_box_local_to_world * vec4(point.xyz, 1.0); 
    // }
    // else {
        w_position = target_transform.localToWorld * vec4(point.xyz, 1.0);
    // }

    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (push.consts.use_multiview == 0) ? push.consts.viewIndex : gl_ViewIndex;
    #endif
    fragTexCoord = texcoord;
    gl_Position = camera.multiviews[viewIndex].viewproj * 
        camera_transform.worldToLocalRotation * 
        camera_transform.worldToLocalTranslation * 
        w_position;
    gl_Position -= vec4(0.0, 0.0, .0001, 0.0);
}
