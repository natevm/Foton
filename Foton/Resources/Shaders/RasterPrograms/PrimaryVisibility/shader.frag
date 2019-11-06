#version 450

#define RASTER
#define PRIMARY_VISIBILITY

/* Inputs */
layout(location = 0) in vec3 w_normal;
layout(location = 1) in vec3 w_position;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 w_cameraPos;
layout(location = 4) in vec3 m_position;
layout(location = 5) in vec2 s_motion;

#include "Foton/Resources/Shaders/Common/Descriptors.hxx"
#include "Foton/Resources/Shaders/Common/ShaderCommon.hxx"

void main() 
{
    randomDimension = 0;

	/* Compute ray origin and direction */
	const ivec2 pixel_coords = ivec2(gl_FragCoord.xy);
	const vec2 pixel_center = vec2(gl_FragCoord.xy) + vec2(random(), random());
	
	/* Rasterizing... Just calculate first hit. */
	vec3 albedo_color = vec3(0.0);
	
	/* We hit an object. Load any additional required data. */ 
	float dist = distance(w_cameraPos, w_position);
	EntityStruct entity;
	MaterialStruct mat; 
    LightStruct light;
    light.color = vec4(0.0);
    light.intensity = 0.f;
	
    unpack_entity_struct(push.consts.target_id, entity);
    unpack_material_struct(push.consts.target_id, entity, fragTexCoord, m_position, mat);
    unpack_light_struct(push.consts.target_id, entity, light);


	albedo_color += mat.base_color.rgb;
	albedo_color += light.color.rgb * light.intensity;
	position_depth = vec4(w_position, dist);
    normal_id = vec4(w_normal, push.consts.target_id);
    seed_luminance = vec4(pixel_coords.x, pixel_coords.y, push.consts.frame, luminance(albedo_color));
    albedo = vec4(albedo_color, 1.0);
    motion = vec4(s_motion, 0.0, 0.0);
    uv = vec4(fragTexCoord.x, fragTexCoord.y, 0.0, 0.0);
}
