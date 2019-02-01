#version 450
#include "Pluto/Resources/Shaders/ShaderCommon.hxx"
#include "Pluto/Resources/Shaders/FragmentCommon.hxx"

// #define PI 3.1415926535897932384626433832795

// float getRoughness(MaterialStruct material)
// {
// 	float roughness = material.roughness;


// 	if (material.roughness_texture_id != -1) {
//   		roughness = texture(sampler2D(texture_2Ds[material.roughness_texture_id], samplers[material.roughness_texture_id]), fragTexCoord).r;
// 	}

// 	return roughness;
// }

// vec2 sampleBRDF(vec3 N, vec3 V, float roughness)
// {
// 	return texture( 
// 			sampler2D(texture_2Ds[push.consts.brdf_lut_id], samplers[push.consts.brdf_lut_id]), 
// 			vec2(max(dot(N, V), 0.0), roughness)
// 	).rg;
// }

// // Todo: add support for sampled irradiance
// vec3 sampleIrradiance(vec3 N)
// {
// 	vec3 irradiance = vec3(0.0, 0.0, 0.0);
// 	vec3 adjusted = vec3(N.x, N.z, N.y);
// 	if (push.consts.diffuse_environment_id != -1) {
// 		irradiance = texture(
// 			samplerCube(texture_cubes[push.consts.diffuse_environment_id], samplers[push.consts.diffuse_environment_id]), 
// 			adjusted
// 		).rgb;
// 	} else {
// 		irradiance = getSky(adjusted);
// 	}
// 	return irradiance;
// }

// // From http://filmicgames.com/archives/75
// vec3 Uncharted2Tonemap(vec3 x)
// {
// 	float A = 0.15;
// 	float B = 0.50;
// 	float C = 0.10;
// 	float D = 0.20;
// 	float E = 0.02;
// 	float F = 0.30;
// 	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
// }

// // Normal Distribution function --------------------------------------
// float D_GGX(float dotNH, float roughness)
// {
// 	float alpha = roughness * roughness;
// 	float alpha2 = alpha * alpha;
// 	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
// 	return (alpha2)/(PI * denom*denom); 
// }

// // Geometric Shadowing function --------------------------------------
// float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
// {
// 	float r = (roughness + 1.0);
// 	float k = (r*r) / 8.0;
// 	float GL = dotNL / (dotNL * (1.0 - k) + k);
// 	float GV = dotNV / (dotNV * (1.0 - k) + k);
// 	return GL * GV;
// }

// // Fresnel function ----------------------------------------------------
// float SchlickR(float cosTheta, float roughness)
// {
// 	return pow(1.0 - cosTheta, 5.0);
// }
// vec3 F_Schlick(float cosTheta, vec3 F0)
// {
// 	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
// }
// vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
// {
// 	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * SchlickR(cosTheta, roughness);
// }


// // Todo: add support for prefiltered reflection 
// vec3 prefilteredReflection(vec3 R, float roughness)
// {
// 	vec3 reflection = vec3(0.0, 0.0, 0.0);
// 	float lod = roughness * 10.0; // Hard coded for now //uboParams.prefilteredCubeMipLevels;
// 	float lodf = floor(lod);
// 	float lodc = ceil(lod);
// 	vec3 adjusted = vec3(R.x, R.z, R.y);

// 	if (push.consts.specular_environment_id != -1) {
	
// 		vec3 a = textureLod(
// 			samplerCube(texture_cubes[push.consts.specular_environment_id], samplers[push.consts.specular_environment_id]), 
// 			adjusted, lodf).rgb;

// 		vec3 b = textureLod(
// 			samplerCube(texture_cubes[push.consts.specular_environment_id], samplers[push.consts.specular_environment_id]), 
// 			adjusted, lodc).rgb;

// 		reflection = mix(a, b, lod - lodf);
// 	}
// 	 else {
// 		reflection = mix(getSky(adjusted), (push.consts.top_sky_color.rgb + push.consts.bottom_sky_color.rgb) * .5, roughness);
// 	}

// 	return reflection;
// }

// vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, vec3 albedo, float metallic, float roughness)
// {
// 	// Precalculate vectors and dot products	
// 	vec3 H = normalize (V + L);
// 	float dotNH = clamp(dot(N, H), 0.0, 1.0);
// 	float dotNV = clamp(dot(N, V), 0.0, 1.0);
// 	float dotNL = clamp(dot(N, L), 0.0, 1.0);

// 	vec3 color = vec3(0.0);

// 	if (dotNL > 0.0) {
// 		// D = Normal distribution (Distribution of the microfacets)
// 		float D = D_GGX(dotNH, roughness); 
// 		// G = Geometric shadowing term (Microfacets shadowing)
// 		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
// 		// F = Fresnel factor (Reflectance depending on angle of incidence)
// 		vec3 F = F_Schlick(dotNV, F0);		
// 		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);		
// 		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);			
// 		color += (kD * albedo / PI + spec) * dotNL;
// 	}

// 	return color;
// }

// // Todo: add support for normal maps
// // See http://www.thetenthplanet.de/archives/1180
// vec3 perturbNormal()
// {
// 	// vec3 tangentNormal = texture(normalMap, inUV).xyz * 2.0 - 1.0;

// 	// vec3 q1 = dFdx(inWorldPos);
// 	// vec3 q2 = dFdy(inWorldPos);
// 	// vec2 st1 = dFdx(inUV);
// 	// vec2 st2 = dFdy(inUV);

// 	// vec3 N = normalize(inNormal);
// 	// vec3 T = normalize(q1 * st2.t - q2 * st1.t);
// 	// vec3 B = -normalize(cross(N, T));
// 	// mat3 TBN = mat3(T, B, N);

// 	// return normalize(TBN * tangentNormal);
// 	return vec3(0.0, 0.0, 0.0);
// }

// void main() {
// 	EntityStruct entity = ebo.entities[push.consts.target_id];
// 	MaterialStruct material = mbo.materials[entity.material_id];

// 	vec3 N = /*(material.hasNormalTexture == 1.0f) ? perturbNormal() :*/ normalize(w_normal);
// 	vec3 V = normalize(w_cameraPos - w_position);
// 	vec3 R = -normalize(reflect(V, N));
	
// 	float eta = 1.0 / material.ior; // air = 1.0 / material = ior
// 	vec3 Refr = normalize(refract(-V, N, eta));

// 	float metallic = material.metallic;
// 	float transmission = material.transmission;
// 	float roughness = getRoughness(material);
// 	float transmission_roughness = material.transmission_roughness;
// 	vec4 albedo = getAlbedo();

// 	/* Todo: read from metallic/roughness texture if one exists. */
// 	vec3 reflection = prefilteredReflection(R, roughness).rgb;
// 	vec3 refraction = prefilteredReflection(Refr, min(transmission_roughness + roughness, 1.0) ).rgb;
// 	vec3 irradiance = sampleIrradiance(N);

// 	/* Transition between albedo and black */
// 	vec3 albedo_mix = mix(vec3(0.04), albedo.rgb, max(metallic, transmission));

// 	/* Transition between refraction and black (metal takes priority) */
// 	vec3 refraction_mix = mix(vec3(0.0), refraction, max((transmission - metallic), 0));
		
// 	/* First compute diffuse (None if metal/glass.) */
// 	vec3 diffuse = irradiance * albedo.rgb;
	
// 	/* Next compute specular. */

// 	vec2 brdf = sampleBRDF(N, V, roughness);
// 	float cosTheta = max(dot(N, V), 0.0);
// 	float s = SchlickR(cosTheta, roughness);

// 	// Add on either a white schlick ring if rough, or nothing. 
// 	vec3 albedo_mix_schlicked = albedo_mix + ((max(vec3(1.0 - roughness), albedo_mix) - albedo_mix) * s);
// 	vec3 refraction_schlicked = refraction_mix * (1.0 - s);
// 	vec3 reflection_schlicked = reflection * (mix(s, 1.0, metallic));
// 	vec3 specular_reflection = reflection_schlicked * (albedo_mix_schlicked * brdf.x + brdf.y); // brdf.x seems to never reach 1.0
// 	vec3 specular_refraction = refraction_schlicked * (albedo_mix * brdf.x); // brdf.y adds frensel like ring, which shouldn't exist on glass

// 	/* This is really subtle. Brightens schlicked areas, but only if not metal. */
// 	vec3 kD = (1.0 - albedo_mix_schlicked) * (1.0 - metallic);

// 	// Ambient occlusion part
// 	float ao = /*(material.hasOcclusionTexture == 1.0f) ? texture(aoMap, inUV).r : */ 1.0f;
// 	vec3 ambient = (kD * diffuse + specular_reflection + specular_refraction + specular_refraction) * ao;

// 	// Iterate over point lights
// 	vec3 finalColor = ambient;
// 	for (int i = 0; i < MAX_LIGHTS; ++i) {
// 		int light_entity_id = push.consts.light_entity_ids[i];
// 		if (light_entity_id == -1) continue;

// 		EntityStruct light_entity = ebo.entities[light_entity_id];
// 		if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) continue;

// 		LightStruct light = lbo.lights[light_entity.light_id];

// 		/* If the object has a light component (fake emission) */
// 		if (light_entity_id == push.consts.target_id) {
// 			finalColor += light.diffuse.rgb;
// 		}
// 		else {
// 			TransformStruct light_transform = tbo.transforms[light_entity.transform_id];

// 			vec3 w_light = vec3(light_transform.localToWorld[3]);
// 			vec3 L = normalize(w_light - w_position);
// 			vec3 Lo = light.diffuse.rgb * specularContribution(L, V, N, albedo_mix, albedo.rgb, metallic, roughness);
// 			finalColor += Lo;
// 		}
// 	}
	
// 	// Tone mapping
// 	finalColor = Uncharted2Tonemap(finalColor * push.consts.exposure);
// 	finalColor = finalColor * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

// 	// Gamma correction
// 	finalColor = pow(finalColor, vec3(1.0f / push.consts.gamma));
	

// 	// Handle emission here...

// //	outColor = vec4(max(dot(N, V), 0.0), max(dot(N, V), 0.0), max(dot(N, V), 0.0), 1.0);//vec4(finalColor, 1.0);
// 	outColor = vec4(finalColor, albedo.a);
// }









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
	EntityStruct target_entity = ebo.entities[push.consts.target_id];
	EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
	MaterialStruct material = mbo.materials[target_entity.material_id];
	TransformStruct transform = tbo.transforms[target_entity.transform_id];

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
		vec4 currentSample = sampleVolume(pos, 1.0);
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