#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/Attributes.hxx"

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    EntityStruct entity = ebo.entities[push.consts.target_id];
    CameraStruct camera = cbo.cameras[push.consts.camera_id];
    MaterialStruct material = mbo.materials[entity.material_id];
    TransformStruct transform = tbo.transforms[entity.transform_id];
    
    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (is_multiview_enabled() == false) ? push.consts.viewIndex : gl_ViewIndex;
    #endif
    gl_Position =  camera.multiviews[viewIndex].viewproj * transform.localToWorld * vec4(point, 1.0);
    fragColor = material.base_color;
    fragTexCoord = texcoord;
}
