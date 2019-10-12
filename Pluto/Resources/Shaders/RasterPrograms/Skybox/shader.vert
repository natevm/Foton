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
layout(location = 4) out vec3 m_position;
layout(location = 5) out vec2 s_motion;

void main() {
    int target_entity_transform_id = ebo.entities[push.consts.target_id].transform_id;
    int camera_entity_transform_id = ebo.entities[push.consts.camera_id].transform_id;
    int camera_entity_camera_id = ebo.entities[push.consts.camera_id].camera_id;

    mat4 cam_localToWorld = tbo.transforms[camera_entity_transform_id].localToWorld;
    mat4 cam_worldToLocal = tbo.transforms[camera_entity_transform_id].worldToLocal;
    mat4 cam_worldToLocalPrev = tbo.transforms[camera_entity_transform_id].worldToLocalPrev;

    mat4 target_localToWorld = tbo.transforms[target_entity_transform_id].localToWorld;
    mat4 target_localToWorldPrev = tbo.transforms[target_entity_transform_id].localToWorldPrev;
    mat4 target_worldToLocal = tbo.transforms[target_entity_transform_id].worldToLocal;

    w_position = vec3(target_localToWorld * vec4(point.xyz, 1.0));
    m_position = point.xyz;
    w_normal = normalize(transpose(mat3(target_worldToLocal)) * normal.xyz);
    w_cameraPos = cam_localToWorld[3].xyz; 
    fragTexCoord = texcoord;

    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (is_multiview_enabled() == false) ? push.consts.viewIndex : gl_ViewIndex;
    #endif
    mat4 viewproj = cbo.cameras[camera_entity_camera_id].multiviews[viewIndex].viewproj;

    vec4 v_p_curr = gl_Position = viewproj * cam_worldToLocal * vec4(w_position, 1.0);
    vec4 v_p_prev = viewproj * cam_worldToLocalPrev * target_localToWorldPrev * vec4(m_position, 1.0);
    s_motion = ((v_p_prev.xyz / v_p_prev.w) - (v_p_curr.xyz / v_p_curr.w)).xy;
}
