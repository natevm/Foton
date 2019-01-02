#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable

#include "Pluto/Resources/Shaders/DescriptorLayout.hxx"

layout(location = 0) in vec3 w_normal;
layout(location = 1) in vec3 w_position;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 w_cameraPos;

layout(location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795
// Todo: allow user to specify exposure and gamma
vec3 getAlbedo()
{
	EntityStruct entity = ebo.entities[push.consts.target_id];
	MaterialStruct material = mbo.materials[entity.material_id];
	vec3 albedo = material.base_color.rgb; 

	if (material.base_color_texture_id != -1) {
  		albedo = texture(sampler2D(texture_2Ds[material.base_color_texture_id], samplers[material.base_color_texture_id]), fragTexCoord).rgb;
	}

	return pow(albedo, vec3(2.2));
}

vec2 sampleBRDF(vec3 N, vec3 V, float roughness)
{
	return texture( 
			sampler2D(texture_2Ds[push.consts.brdf_lut_id], samplers[push.consts.brdf_lut_id]), 
			vec2(max(dot(N, V), 0.0), roughness)
	).rg;
}

// Todo: add support for sampled irradiance
vec3 sampleIrradiance(vec3 N)
{
	// vec3 irradiance = vec3(0.0, 0.0, 0.0);
	// if (push.consts.diffuse_environment_id != -1) {
	// 	irradiance = texture(
	// 		samplerCube(texture_cubes[push.consts.diffuse_environment_id], samplers[push.consts.diffuse_environment_id]), 
	// 		N
	// 	).rgb;
	// }
	// return irradiance;
	return vec3(0.01, 0.01, 0.01);
}

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// Todo: add support for prefiltered reflection 
vec3 prefilteredReflection(vec3 R, float roughness)
{
	// float lod = roughness * uboParams.prefilteredCubeMipLevels;
	// float lodf = floor(lod);
	// float lodc = ceil(lod);
	// vec3 a = textureLod(prefilteredMap, R, lodf).rgb;
	// vec3 b = textureLod(prefilteredMap, R, lodc).rgb;
	// return mix(a, b, lod - lodf);
	return vec3(0.01, 0.01, 0.01);
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0) {
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, F0);		
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);		
		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);			
		color += (kD * getAlbedo() / PI + spec) * dotNL;
	}

	return color;
}

// Todo: add support for normal maps
// See http://www.thetenthplanet.de/archives/1180
vec3 perturbNormal()
{
	// vec3 tangentNormal = texture(normalMap, inUV).xyz * 2.0 - 1.0;

	// vec3 q1 = dFdx(inWorldPos);
	// vec3 q2 = dFdy(inWorldPos);
	// vec2 st1 = dFdx(inUV);
	// vec2 st2 = dFdy(inUV);

	// vec3 N = normalize(inNormal);
	// vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	// vec3 B = -normalize(cross(N, T));
	// mat3 TBN = mat3(T, B, N);

	// return normalize(TBN * tangentNormal);
	return vec3(0.0, 0.0, 0.0);
}

void main() {
	EntityStruct entity = ebo.entities[push.consts.target_id];
	MaterialStruct material = mbo.materials[entity.material_id];

	vec3 N = /*(material.hasNormalTexture == 1.0f) ? perturbNormal() :*/ normalize(w_normal);
	vec3 V = normalize(w_cameraPos - w_position);
	vec3 R = normalize(reflect(V, N));

	float metallic = material.metallic;
	float roughness = material.roughness;

	/* Todo: read from metallic/roughness texture if one exists. */

	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, getAlbedo(), metallic);

	// Iterate over point lights
	vec3 finalColor = vec3(0.0, 0.0, 0.0);
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		int light_entity_id = push.consts.light_entity_ids[i];
		if (light_entity_id == -1) continue;

		EntityStruct light_entity = ebo.entities[light_entity_id];
		if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) continue;

		LightStruct light = lbo.lights[light_entity.light_id];
		TransformStruct light_transform = tbo.transforms[light_entity.transform_id];

		vec3 w_light = vec3(light_transform.localToWorld[3]);
		vec3 L = normalize(w_light - w_position);
		vec3 Lo = light.diffuse.rgb * specularContribution(L, V, N, F0, metallic, roughness);

		vec2 brdf = sampleBRDF(N, V, roughness);
		vec3 reflection = prefilteredReflection(R, roughness).rgb;
		vec3 irradiance = sampleIrradiance(N);

		// Diffuse based on irradiance
		vec3 diffuse = irradiance * getAlbedo();

		vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

		// Specular reflectance
		vec3 specular = reflection * (F * brdf.x + brdf.y);

		// Ambient part
		vec3 kD = 1.0 - F;
		kD *= 1.0 - metallic;
		float ao = /*(material.hasOcclusionTexture == 1.0f) ? texture(aoMap, inUV).r : */ 1.0f;
		vec3 ambient = (kD * diffuse + specular) * ao;
		vec3 color = ambient + Lo;

		// Tone mapping
		color = Uncharted2Tonemap(color * push.consts.exposure);
		color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

		// Gamma correction
		color = pow(color, vec3(1.0f / push.consts.gamma));

		finalColor += color;
	}

	// Handle emission here...

	outColor = vec4(finalColor, 1.0);
}