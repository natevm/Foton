#version 450
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

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
    
    #if DISABLE_MULTIVIEW
    int viewIndex = 0;
    #else
    int viewIndex = gl_ViewIndex;
    #endif
    gl_Position =  camera.multiviews[viewIndex].proj * camera.multiviews[viewIndex].view * transform.localToWorld * vec4(point, 1.0);
    gl_PointSize = 1.0;
    fragColor = material.base_color;
    fragTexCoord = texcoord;
}
