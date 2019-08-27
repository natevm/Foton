#version 450

#define RASTER
#define PRIMARY_VISIBILITY

/* Inputs */
layout(location = 0) in vec3 w_normal;
layout(location = 1) in vec3 w_position;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 w_cameraPos;
layout(location = 4) in vec4 vert_color;
layout(location = 5) in vec3 m_position;
layout(location = 6) in vec3 w_cameraDir;
layout(location = 7) in vec3 m_normal;
layout(location = 8) in vec4 s_position;
layout(location = 9) in vec4 s_position_prev;

#include "Pluto/Resources/Shaders/Common/Descriptors.hxx"
#include "Pluto/Resources/Shaders/Common/ShaderCommon.hxx"

void main() 
{
	/* These are required for shading purposes. Wait until they're ready. */
    if ((push.consts.ltc_mat_lut_id < 0) || (push.consts.ltc_mat_lut_id >= MAX_TEXTURES)) return;
    if ((push.consts.ltc_amp_lut_id < 0) || (push.consts.ltc_amp_lut_id >= MAX_TEXTURES)) return;
    randomDimension = 0;

	/* Compute ray origin and direction */
	const ivec2 pixel_coords = ivec2(gl_FragCoord.xy);
	const vec2 pixel_center = vec2(gl_FragCoord.xy) + vec2(random(pixel_coords, push.consts.frame), random(pixel_coords, push.consts.frame));
	const vec2 in_uv = pixel_center / vec2(push.consts.width, push.consts.height);
	vec2 d = in_uv * 2.0 - 1.0; d.y *= -1.0;

	EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
    CameraStruct camera = cbo.cameras[camera_entity.camera_id];
    TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];

	vec3 origin = (camera_transform.localToWorld * camera.multiviews[push.consts.viewIndex].viewinv * vec4(0,0,0,1)).xyz;
	vec3 target = (camera.multiviews[push.consts.viewIndex].projinv * vec4(d.x, d.y, 1, 1)).xyz ;
    vec3 direction = (camera_transform.localToWorld * camera.multiviews[push.consts.viewIndex].viewinv * vec4(normalize(target), 0)).xyz ;
    direction = normalize(direction);

	/* Rasterizing... Just calculate first hit. */
	vec3 albedo_color = vec3(0.0);
	
	/* We hit an object. Load any additional required data. */ 
	float dist = distance(origin, w_position);
	EntityStruct entity; TransformStruct transform; 
	MaterialStruct mat; LightStruct light;
	unpack_entity(push.consts.target_id, entity, transform, mat, light);

	/* Compute motion vector for primary visibility */
	vec3 motion_vector = vec3(0.0);
    bool reset = true;
    if (entity.initialized == 1) {
        reset = false;
        TransformStruct transform = tbo.transforms[entity.transform_id];
        vec4 w_p_prev = transform.localToWorldPrev * vec4(m_position, 1.0);

        vec4 v_p_curr = camera.multiviews[push.consts.viewIndex].viewproj * camera_transform.worldToLocal * vec4(w_position, 1.0);
        vec4 v_p_prev = camera.multiviews[push.consts.viewIndex].viewproj * camera_transform.worldToLocalPrev * w_p_prev;

        motion_vector = (v_p_prev.xyz / v_p_prev.w) - (v_p_curr.xyz / v_p_curr.w);
    }

	albedo_color += mat.base_color.rgb;
	position_depth = vec4(w_position, dist);
    normal_id = vec4(w_normal, push.consts.target_id);
    seed_luminance = vec4(pixel_coords.x, pixel_coords.y, push.consts.frame, luminance(albedo_color));
    albedo = vec4(albedo_color, 1.0);
    motion = vec4(motion_vector, (reset) ? 1.0 : 0.0);
    uv = vec4(fragTexCoord.x, fragTexCoord.y, 0.0, 0.0);
}
