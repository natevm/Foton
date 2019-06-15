#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/Attributes.hxx"
#include "Pluto/Resources/Shaders/VertexVaryings.hxx"

void main() {
    EntityStruct target_entity = ebo.entities[push.consts.target_id];
    EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];
    TransformStruct target_transform = tbo.transforms[target_entity.transform_id];

    vec3 w_position;
    if (push.consts.show_bounding_box == 1) {
        MeshStruct mesh_struct = mesh_ssbo.meshes[target_entity.mesh_id];
        w_position = vec3(target_transform.localToWorld * mesh_struct.bb_local_to_parent * vec4(point.xyz, 1.0));
    }
    else w_position = vec3(target_transform.localToWorld * vec4(point.xyz, 1.0));

    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (is_multiview_enabled() == false) ? push.consts.viewIndex : gl_ViewIndex;
    #endif
    gl_Position = camera.multiviews[viewIndex].viewproj * 
        camera_transform.worldToLocalRotation * 
        camera_transform.worldToLocalTranslation * 
        vec4(w_position, 1.0);
    
    /* Epsilon offset to account for depth precision errors values */
    #ifndef DISABLE_REVERSE_Z
    gl_Position -= vec4(0.0, 0.0, .001, 0.0);
    #else
    gl_Position += vec4(0.0, 0.0, .001, 0.0);
    #endif
}
