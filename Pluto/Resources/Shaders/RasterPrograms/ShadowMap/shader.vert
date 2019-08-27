#version 450
#include "Pluto/Resources/Shaders/Common/Descriptors.hxx"
#include "Pluto/Resources/Shaders/Common/Attributes.hxx"
#include "Pluto/Resources/Shaders/Common/Options.hxx"

layout(location = 0) out vec3 w_position;
layout(location = 1) out vec3 c_position;

void main() {
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    EntityStruct target_entity = ebo.entities[push.consts.target_id];
    EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];

    TransformStruct target_transform = tbo.transforms[target_entity.transform_id];

    // if (push.consts.show_bounding_box > 0) {
    //     w_position = (push.consts.bounding_box_local_to_world * vec4(point.xyz, 1.0)).xyz; 
    // }
    // else {
        w_position = (target_transform.localToWorld * vec4(point.xyz, 1.0)).xyz;
    // }

    c_position = (camera_transform.localToWorld[3]).xyz;

    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (is_multiview_enabled() == false) ? push.consts.viewIndex : gl_ViewIndex;
    #endif

    gl_Position = camera.multiviews[viewIndex].viewproj * 
        camera_transform.worldToLocalRotation * camera_transform.worldToLocalTranslation * vec4(w_position, 1.0);
}
