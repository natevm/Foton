#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/Attributes.hxx"
#include "Pluto/Resources/Shaders/VertexVaryings.hxx"
#include "Pluto/Resources/Shaders/Random.hxx"
#include "Pluto/Resources/Shaders/Options.hxx"

void main() {
    EntityStruct target_entity = ebo.entities[push.consts.target_id];
    EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];
    TransformStruct target_transform = tbo.transforms[target_entity.transform_id];

    w_position = vec3(target_transform.localToWorld * vec4(point.xyz, 1.0));
    w_normal = normalize(transpose(mat3(target_transform.worldToLocal)) * normal.xyz);
    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (is_multiview_enabled() == false) ? push.consts.viewIndex : gl_ViewIndex;
    #endif
    w_cameraPos = vec3(camera.multiviews[viewIndex].viewinv[3]) + vec3(camera_transform.localToWorld[3]);

    fragTexCoord = texcoord;

    mat4 camWorldToLocal = camera_transform.worldToLocal;
    gl_Position = camera.multiviews[viewIndex].viewproj * camWorldToLocal * vec4(w_position, 1.0);
    m_position = point.xyz;
    vert_color = color;
}
