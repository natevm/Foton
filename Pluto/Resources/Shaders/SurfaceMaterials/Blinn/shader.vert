#version 450
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

layout(location = 0) in vec3 point;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texcoord;

layout(location = 0) out vec3 w_normal;
layout(location = 1) out vec3 w_position;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 w_reflection;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    EntityStruct target_entity = ebo.entities[push.consts.target_id];
    EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];

    MaterialStruct material = mbo.materials[target_entity.material_id];
    TransformStruct target_transform = tbo.transforms[target_entity.transform_id];

    w_position = vec3(target_transform.localToWorld * vec4(point.xyz, 1.0));
    #if DISABLE_MULTIVIEW
    int viewIndex = 0;
    #else
    int viewIndex = gl_ViewIndex;
    #endif
    vec3 w_cameraPos = vec3(camera.multiviews[viewIndex].viewinv[3]) + vec3(camera_transform.localToWorld[3]); // could be wrong.
    vec3 w_viewDir = w_position - w_cameraPos;
    
    w_normal = normalize(vec3(target_transform.localToWorld * vec4(normal, 0)));
    vec3 reflection = reflect(w_viewDir, normalize(normal));
    
    /* From z up to y up */ // why am i doing this?...
    w_reflection.x = reflection.x;
    w_reflection.y = reflection.z;
    w_reflection.z = reflection.y;

    gl_Position = camera.multiviews[viewIndex].proj * camera.multiviews[viewIndex].view * camera_transform.worldToLocal * vec4(w_position, 1.0);
    fragTexCoord = texcoord;
}
