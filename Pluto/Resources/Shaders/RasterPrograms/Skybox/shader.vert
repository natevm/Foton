#version 450
#include "Pluto/Resources/Shaders/Common/Descriptors.hxx"
#include "Pluto/Resources/Shaders/Common/Attributes.hxx"
#include "Pluto/Resources/Shaders/Common/Random.hxx"
#include "Pluto/Resources/Shaders/Common/Options.hxx"

/* Outputs */
layout(location = 0) out vec3 w_normal;
layout(location = 1) out vec3 w_position;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 w_cameraPos;
layout(location = 4) out vec4 vert_color;
layout(location = 5) out vec3 m_position;
layout(location = 6) out vec3 w_cameraDir;
layout(location = 7) out vec3 m_normal;
layout(location = 8) out vec4 s_position;
layout(location = 9) out vec4 s_position_prev;

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
