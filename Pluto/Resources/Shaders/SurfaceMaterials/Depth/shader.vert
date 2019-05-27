#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/Attributes.hxx"

layout(location = 0) out vec3 w_position;
layout(location = 1) out vec3 c_position;

void main() {
    EntityStruct target_entity = ebo.entities[push.consts.target_id];
    EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];

    TransformStruct target_transform = tbo.transforms[target_entity.transform_id];

    // w_position = (target_transform.localToWorld * vec4(point.xyz, 1.0)).xyz;

    
    if (push.consts.show_bounding_box == 1) {
        MeshStruct mesh_struct = mesh_ssbo.meshes[target_entity.mesh_id];
        w_position = vec3(target_transform.localToWorld * mesh_struct.bb_local_to_parent * vec4(point.xyz, 1.0));
    }
    else w_position = vec3(target_transform.localToWorld * vec4(point.xyz, 1.0));

    c_position = (camera_transform.localToWorld[3]).xyz;

    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (is_multiview_enabled() == false) ? push.consts.viewIndex : gl_ViewIndex;
    #endif
    gl_Position = camera.multiviews[viewIndex].viewproj * camera_transform.worldToLocalRotation * camera_transform.worldToLocalTranslation * vec4(w_position, 1.0);
}
