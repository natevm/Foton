#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/FragmentVaryings.hxx"
#include "Pluto/Resources/Shaders/FragmentCommon.hxx"

void main() {
	EntityStruct entity = ebo.entities[push.consts.target_id];
	MaterialStruct material = mbo.materials[entity.material_id];

	float alphaMask = getAlphaMask(material);

	vec3 N = /*(material.hasNormalTexture == 1.0f) ? perturbNormal() :*/ normalize(w_normal);
	if (!gl_FrontFacing) N *= -1.0;
	vec3 V = normalize(w_cameraPos - w_position);
	vec3 R = -normalize(reflect(V, N));	
	float eta = 1.0 / material.ior; // air = 1.0 / material = ior
	vec3 Refr = normalize(refract(-V, N, eta));

	float metallic = material.metallic;
	float transmission = material.transmission;
	float roughness = getRoughness(material);
	float transmission_roughness = getTransmissionRoughness(material);
	vec4 albedo = getAlbedo(material);
	float alpha = (alphaMask < 1.0) ? alphaMask : albedo.a; // todo: move alpha out of albedo. 
	if (alpha == 0.0) discard;

	/* Todo: read from metallic/roughness texture if one exists. */
	vec3 reflection = getPrefilteredReflection(R, roughness).rgb;
	vec3 refraction = getPrefilteredReflection(Refr, min(transmission_roughness + roughness, 1.0) ).rgb;
	vec3 irradiance = sampleIrradiance(N);

	/* Transition between albedo and black */
	vec3 albedo_mix = mix(vec3(0.04), albedo.rgb, max(metallic, transmission));

	/* Transition between refraction and black (metal takes priority) */
	vec3 refraction_mix = mix(vec3(0.0), refraction, max((transmission - metallic), 0));
		
	/* First compute diffuse (None if metal/glass.) */
	vec3 diffuse = irradiance * albedo.rgb;
	
	/* Next compute specular. */
	vec2 brdf = sampleBRDF(N, V, roughness);
	float cosTheta = max(dot(N, V), 0.0);
	float s = SchlickR(cosTheta, roughness);

	// Add on either a white schlick ring if rough, or nothing. 
	vec3 albedo_mix_schlicked = albedo_mix + ((max(vec3(1.0 - roughness), albedo_mix) - albedo_mix) * s);
	vec3 refraction_schlicked = refraction_mix * (1.0 - s);
	vec3 reflection_schlicked = reflection * (mix(s, 1.0, metallic));
	vec3 specular_reflection = reflection_schlicked * (albedo_mix_schlicked * brdf.x + brdf.y); // brdf.x seems to never reach 1.0
	vec3 specular_refraction = refraction_schlicked * (albedo_mix * brdf.x); // brdf.y adds frensel like ring, which shouldn't exist on glass

	/* This is really subtle. Brightens schlicked areas, but only if not metal. */
	vec3 kD = (1.0 - albedo_mix_schlicked) * (1.0 - metallic);

	// Ambient occlusion part
	float ao = /*(material.hasOcclusionTexture == 1.0f) ? texture(aoMap, inUV).r : */ 1.0f;
	vec3 ambient = (kD * diffuse + specular_reflection + specular_refraction + specular_refraction) * ao;

	// Iterate over point lights
	vec3 finalColor = ambient + get_light_contribution(w_position, V, N, albedo_mix, albedo.rgb, metallic, roughness);

	// Tone mapping
	finalColor = Uncharted2Tonemap(finalColor * push.consts.exposure);
	finalColor = finalColor * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

	// Gamma correction
	finalColor = pow(finalColor, vec3(1.0f / push.consts.gamma));
	
	// Handle emission here...
	outColor = vec4(finalColor, alpha);
}
