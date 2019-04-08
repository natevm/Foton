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

    w_position = vec3(target_transform.localToWorld * vec4(point.xyz, 1.0));
    w_normal = normalize(transpose(target_transform.worldToLocal) * vec4(normal, 0.0)).xyz;
    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (push.consts.use_multiview == 0) ? push.consts.viewIndex : gl_ViewIndex;
    #endif
    w_cameraPos = vec3(camera.multiviews[viewIndex].viewinv[3]) + vec3(camera_transform.localToWorld[3]);

    fragTexCoord = texcoord;
    gl_Position = camera.multiviews[viewIndex].viewproj * camera_transform.worldToLocal * vec4(w_position, 1.0);

    m_position = point;
    vert_color = color;
}
