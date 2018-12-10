#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform PerspectiveBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewinv;
    mat4 projinv;
    float near;
    float far;
} pbo;

layout(binding = 1) uniform TransformBufferObject{
    mat4 worldToLocal;
    mat4 localToWorld;
} tbo;

layout(binding = 2) uniform MaterialBufferObject {
	int numSamples;
	float lod;
} mbo;

layout(binding = 3) uniform sampler3D volumeTexture;


layout(location = 0) in vec3 ray_destination;
layout(location = 0) out vec4 outColor;

struct Ray {
	vec3 origin;
	vec3 dir;
};

struct AABB {
	vec3 minimum;
	vec3 maximum;
};

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
	vec4 ray_origin_world_pos = pbo.viewinv[3];
	vec3 ray_origin = vec3(tbo.worldToLocal * ray_origin_world_pos);

	/* Create ray from eye to fragment */
	vec3 ray_direction = ray_destination - ray_origin;
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
	step_size += step_size * random * .15;

	vec3 pos = ray_start;
	vec3 step = normalize(ray_stop-ray_start) * step_size;
	float travel = distance(ray_stop, ray_start);
	for (int i=0; i < samples && travel > 0.0; ++i, pos += step, travel -= step_size) {
		vec4 currentSample = textureLod(volumeTexture, pos, mbo.lod);
		alpha = alpha + currentSample.a * (1 - alpha);
		if (alpha < 1.0)
			color = color + (1.0 - alpha) * currentSample.rgb;
		if (alpha > 1.0) {
			alpha = 1.0;
			break;
		}
	}
	outColor = vec4(color, alpha);
}