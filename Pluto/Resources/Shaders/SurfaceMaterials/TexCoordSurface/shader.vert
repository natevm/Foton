#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/Attributes.hxx"

layout(location = 0) out vec4 fragColor;

void main() {
    EntityStruct target_entity = ebo.entities[push.consts.target_id];
    EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];

    MaterialStruct material = mbo.materials[target_entity.material_id];
    TransformStruct target_transform = tbo.transforms[target_entity.transform_id];

    vec3 w_position = vec3(target_transform.localToWorld * vec4(point.xyz, 1.0));
    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (push.consts.use_multiview == 0) ? push.consts.viewIndex : gl_ViewIndex;
    #endif
    gl_Position = camera.multiviews[viewIndex].viewproj * camera_transform.worldToLocal * vec4(w_position, 1.0);
    fragColor = vec4(texcoord.x, 0.0, texcoord.y, 1.0);
}
