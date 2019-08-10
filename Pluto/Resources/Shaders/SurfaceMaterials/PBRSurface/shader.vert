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


    // w_position = vec3(target_transform.localToWorld * vec4(point.xyz, 1.0));
    vec3 w_position_prev;

    if (show_bounding_box()) {
        MeshStruct mesh_struct = mesh_ssbo.meshes[target_entity.mesh_id];
        w_position = vec3(target_transform.localToWorld * mesh_struct.bb_local_to_parent * vec4(point.xyz, 1.0));
        w_position_prev = vec3(target_transform.localToWorldPrev * mesh_struct.bb_local_to_parent * vec4(point.xyz, 1.0));
    }
    else {
        w_position = vec3(target_transform.localToWorld * vec4(point.xyz, 1.0));
        w_position_prev = vec3(target_transform.localToWorldPrev * vec4(point.xyz, 1.0));
    }

    w_normal = normalize((transpose(target_transform.worldToLocal) * vec4(normal.xyz, 0.0)).xyz);
    m_normal = normalize(normal.xyz);

    #ifdef DISABLE_MULTIVIEW
    int viewIndex = push.consts.viewIndex;
    #else
    int viewIndex = (is_multiview_enabled() == false) ? push.consts.viewIndex : gl_ViewIndex;
    #endif
    // Note, we cant account for view matrix translation using this: vec3(camera.multiviews[viewIndex].viewinv[3])
    // Use RTX stuff for reflection debugging.
    w_cameraPos = vec3(camera_transform.localToWorld[3]); 
    w_cameraDir = normalize(w_cameraPos - w_position);

    fragTexCoord = texcoord;

    s_position = gl_Position = camera.multiviews[viewIndex].viewproj * camera_transform.worldToLocal * vec4(w_position, 1.0);
    s_position_prev = camera.multiviews[viewIndex].viewproj * camera_transform.worldToLocalPrev * vec4(w_position_prev, 1.0);
    m_position = point.xyz;
    vert_color = color;
    
    /* If we're using TAA, jitter the camera to generate noise which we can temporally resolve */
    if (is_taa_enabled()) {
        float randNowX  = (samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(0, 0, push.consts.frame, 0) - .5) / push.consts.width;
        float randNowY  = (samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(0, 0, push.consts.frame, 1) - .5) / push.consts.height;
        float randPrevX = (samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(0, 0, push.consts.frame - 1, 0) - .5) / push.consts.width;
        float randPrevY = (samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(0, 0, push.consts.frame - 1, 1) - .5) / push.consts.height;
        s_position += 2.0 * vec4(randNowX, randNowY, 0.0, 0.0);
        s_position_prev += 2.0 * vec4(randPrevX, randPrevY, 0.0, 0.0);
    }
}
