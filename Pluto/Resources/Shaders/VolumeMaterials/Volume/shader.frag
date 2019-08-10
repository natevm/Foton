#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/FragmentVaryings.hxx"
#include "Pluto/Resources/Shaders/FragmentCommon.hxx"
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"

bool IntersectBox(Ray r, AABB aabb, out float t0, out float t1) 
{
	vec3 invR = 1.0 / r.dir;
	vec3 tbot = invR * (aabb.minimum-r.origin);
		vec3 ttop = invR * (aabb.maximum-r.origin);
		vec3 tmin = min(ttop, tbot);
		vec3 tmax = max(ttop, tbot);
		vec2 t = max(tmin.xx, tmin.yz);
		t0 = max(t.x, t.y);
		t = min(tmax.xx, tmax.yz);
		t1 = min(t.x, t.y);
		return t0 <= t1;
}

float rand(vec2 co) {
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

const int samples = 128;
const float maxDist = sqrt(2.0);
float step_size = maxDist/float(samples);

void main() {
	EntityStruct target_entity = ebo.entities[push.consts.target_id];
	EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
	MaterialStruct material = mbo.materials[target_entity.material_id];
	TransformStruct transform = tbo.transforms[target_entity.transform_id];

	bool use_transfer_function = ((material.transfer_function_texture_id >= 0) && (material.transfer_function_texture_id < MAX_TEXTURES));

	vec3 ray_origin = vec3(transform.worldToLocal * vec4(w_cameraPos, 1.0));

	/* Create ray from eye to fragment */
	vec3 ray_direction = m_position - ray_origin;
	Ray eye = Ray(ray_origin, normalize(ray_direction));

	/* Intersect the ray with an object space box. */
	AABB aabb = AABB(vec3(-1.0), vec3(+1.0));
	float tnear, tfar;
	bool intersected = IntersectBox(eye, aabb, tnear, tfar);
	if (!intersected) discard;
	tnear = max(tnear, 0.0);

	vec3 ray_start = eye.origin + eye.dir * tnear;
	vec3 ray_stop = eye.origin + eye.dir * tfar;

	// Transform from object space to texture coordinate space:
	ray_start = (ray_start + 1.0) * 0.50;
	ray_stop = (ray_stop + 1.0) * 0.50;

	// Perform the sequential ray marching:
	vec3 color = vec3(0.0, 0.0, 0.0);
	float alpha = 0.0;
	
	float random = rand(gl_FragCoord.xy);
	step_size += step_size * random * .05;

	vec3 pos = ray_start;
	vec3 step = normalize(ray_stop-ray_start) * step_size;
	float travel = distance(ray_stop, ray_start);
	for (int i=0; i < samples && travel > 0.0; ++i, pos += step, travel -= step_size) {
		vec4 currentSample = (use_transfer_function) ? 
			texture(sampler2D(texture_2Ds[material.transfer_function_texture_id], samplers[0]), 
				vec2(sample_texture_3D(vec4(1.0, 1.0, 1.0, 1.0), material.volume_texture_id, pos, 1.0).r, 0.5) ) : 
					sample_texture_3D(vec4(1.0, 1.0, 1.0, 1.0), material.volume_texture_id, pos, 1.0);
		
		// color = color + (1.0 - alpha) * cx.rgb * cx.a;
		// alpha = alpha + cx.a * (1 - alpha);

		// currentSample.a *= .01;
				
		alpha = alpha + currentSample.a * (1.0 - alpha);
		if (alpha < 1.0) {
			color = color + (1.0 - alpha) * currentSample.rgb * currentSample.a;
		}
		if (alpha > 1.0) {
			alpha = 1.0;
			break;
		}
	}
	outColor = vec4(color, alpha);
	GBUF1 = vec4(0.0f);
}