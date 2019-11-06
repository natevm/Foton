#ifndef SHADER_COMMON
#define SHADER_COMMON

#include "Foton/Resources/Shaders/Common/LTCCommon.hxx"
#include "Foton/Resources/Shaders/Common/ShaderConstants.hxx"
#include "Foton/Resources/Shaders/Common/Random.hxx"
#include "Foton/Resources/Shaders/Common/Halton.hxx"

#include "Foton/Resources/Shaders/Common/DisneyBRDF.hxx"
#include "Foton/Resources/Shaders/Common/DisneyBSDF.hxx"
#include "Foton/Resources/Shaders/Common/Lights.hxx"
#include "Foton/Resources/Shaders/Common/Utilities.hxx"



// BRDF Lookup Tables --------------------------------------
vec2 sampleBRDF(vec3 N, vec3 V, float roughness)
{
    if ((push.consts.brdf_lut_id < 0) || (push.consts.brdf_lut_id >= max_textures))
        return vec2(0.0, 0.0);

    TextureStruct tex = txbo.textures[push.consts.brdf_lut_id];

    if ((tex.sampler_id < 0) || (tex.sampler_id >= max_samplers)) 
        return vec2(0.0, 0.0);

	return texture( 
			sampler2D(texture_2Ds[push.consts.brdf_lut_id], samplers[tex.sampler_id]), 
			vec2(max(dot(N, V), 0.0), 1.0 - roughness)
	).rg;
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
float SchlickR(float cosTheta, float roughness)
{
	return pow(1.0 - cosTheta, 5.0);
}
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * SchlickR(cosTheta, roughness);
}

float LazanyiSchlick(float cosTheta, float reflectivity, float F82) {
    // From Naty Hoffman at Lucasfilm
    // F82 represents fresnel reflectance at the peak angle.
    float theta_max = 1.425934; // arccos(1/7) = 81.7;
    float cosTheta_max = cos(theta_max);
    float h = F82;
    float r = reflectivity;
    float alpha = ( r + (1.0 - r) * pow((cosTheta_max), 5) - h ) / (cosTheta_max * pow((1.0 - cosTheta_max), 6));
    return r + (1.0 - r) * pow((1.0 - cosTheta), 5) - alpha * cosTheta * pow((1.0 - cosTheta), 6);
}

// Specular Contribution function --------------------------------------
vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, vec3 albedo, float metallic, float roughness)
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
		color += (kD * albedo / PI + spec) * dotNL;
	}

	return color;
}

// Shadows          ----------------------------------------------------
float get_shadow_contribution(EntityStruct light_entity, LightStruct light, vec3 w_light_position, vec3 w_position) 
{
    CameraStruct light_camera = cbo.cameras[light_entity.camera_id];
    int tex_id = light_camera.multiviews[0].tex_id;

    TextureStruct tex = txbo.textures[tex_id];
    if ((tex.sampler_id < 0) || (tex.sampler_id >= max_textures)) return 1.0;

    TransformStruct light_transform = tbo.transforms[light_entity.transform_id];
    vec3 l_position = (light_transform.worldToLocalRotation * light_transform.worldToLocalTranslation * vec4(w_position, 1.0)).xyz;
    vec3 pos_dir = -normalize(l_position);
    float l_dist = length(l_position);
    vec3 adjusted = vec3(pos_dir.x, -pos_dir.z, pos_dir.y);

    float shadow = 0.0;
    float bias = .1;
    float samples = light.softnessSamples;
    float viewDistance = l_dist;
    float diskRadius = light.softnessRadius * clamp((1.0 / pow(l_dist + 1.0, 3.)), .002, .1);

    // PCF
    if ((light.flags & (1 << 3)) == 0 )
    {
        for (int i = 0; i < samples; i++)
        {
            float closestDepth = texture(
                samplerCube(texture_cubes[tex_id], samplers[tex.sampler_id]), 
                adjusted + sampleOffsetDirections[i] * diskRadius
            ).r;

            if (closestDepth == 0.0f) return 1.0f; // testing 

            shadow += 1.0f;
            if((l_dist - closestDepth) > 0.15)
                shadow -= min(1.0, pow((l_dist - closestDepth), .1));
        }
        shadow /= float(samples);
    }

    // VSM
    else {

        // We retrive the two moments previously stored (depth and depth*depth)
        vec2 moments = texture(
            samplerCube(texture_cubes[tex_id], samplers[tex.sampler_id]), 
            adjusted
        ).rg;

        // Surface is fully lit. as the current fragment is before the light occluder
        if ((moments.x - l_dist) > -0.15)
            return 1.0 ;


        // The fragment is either in shadow or penumbra. We now use chebyshev's upperBound to check
        // How likely this pixel is to be lit (p_max)
        float variance = moments.y - (moments.x*moments.x);
        variance = max(variance,0.00002);

        float d = l_dist - moments.x;

        float p_max = variance / (variance + d*d);
    
        return p_max;
    }


    // if (tex.sampler_id != -1) {
    //     shadow = texture(
    //         samplerCube(texture_cubes[tex_id], samplers[tex.sampler_id]), adjusted
    //     ).r;

    //     shadow = (shadow + bias < l_dist) ? 0.0 : 1.0;
    // }

    return shadow;
}

// Dome Lighting --------------------------------------
vec3 getSky(vec3 dir)
{
	float transition = push.consts.sky_transition;

    // float height = max(dot(normalize(dir), vec3(0.0, 1.0, 0.0)) + transition, 0.0); 
	// float alpha = min(height / (transition * 2.0), 1.0);//, 10.0);

    float height = dot(dir, vec3(0.0, 1.0, 0.0));
    float alpha = 1.0 / (1.0 + pow(2.717, -push.consts.sky_transition * height));
    return sqrt(  alpha * pow(push.consts.top_sky_color.rgb, vec3(2.0, 2.0, 2.0)) + (1.0 - alpha) * pow(push.consts.bottom_sky_color.rgb, vec3(2.0, 2.0, 2.0)));
}

vec3 getSun(vec3 dir){
	float height = dot(dir, vec3(0.0, 1.0, 0.0));

	vec3 sun_dir = normalize(vec3(1., 1., 1.));

	float sun = max(dot(dir, sun_dir), 0.0);//distance(uv, vec2(cos(push.consts.time), sin(push.consts.time)));
    sun = clamp(sun,0.0,1.0);
    
    float glow = sun;
    glow = clamp(glow,0.0,1.0);
    
    sun = pow(sun,100.0);
    sun *= 100.0;
    sun = clamp(sun,0.0,1.0);
    
    glow = pow(glow,6.0) * 1.0;
    glow = pow(glow,(height));
    glow = clamp(glow,0.0,1.0);
    
    sun *= pow(dot(height, height), 1.0 / 1.65);
    
    glow *= pow(dot(height, height), 1.0 / 2.0);
    
    sun += glow;
    
    vec3 sunColor = vec3(1.0,0.6,0.05) * sun;
    
    return vec3(sunColor);
}

vec3 get_environment_color(vec3 dir) {
	vec3 adjusted = vec3(dir.x, dir.z, dir.y);
	if (push.consts.environment_id != -1) {
		TextureStruct tex = txbo.textures[push.consts.environment_id];

		float lod = push.consts.environment_roughness * max(tex.mip_levels, 0.0);
		float lodf = floor(lod);
		float lodc = ceil(lod);
		
		vec3 a = textureLod(
			samplerCube(texture_cubes[push.consts.environment_id], samplers[tex.sampler_id]), 
			adjusted, lodf).rgb;

		vec3 b = textureLod(
			samplerCube(texture_cubes[push.consts.environment_id], samplers[tex.sampler_id]), 
			adjusted, lodc).rgb;

		return mix(a, b, lod - lodf);
	}

    return getSky(adjusted);


	// vec3 up = vec3(0.0, 1.0, 0.0);
	// float alpha = dot(up, dir);
	// return vec3(alpha, alpha, alpha);
}

vec3 sampleIrradiance(vec3 N)
{
    if ((push.consts.diffuse_environment_id < 0 ) || (push.consts.diffuse_environment_id >= max_textures))
	    return getSky(N.xzy);

    TextureStruct tex = txbo.textures[push.consts.diffuse_environment_id];
    
    if ((tex.sampler_id < 0 ) || (tex.sampler_id >= max_samplers))
        return getSky(N.xzy);

    return texture(
        samplerCube(texture_cubes[push.consts.diffuse_environment_id], samplers[tex.sampler_id]), N.xzy
    ).rgb;
}

vec3 getPrefilteredReflection(vec3 R, float roughness)
{
	if ((push.consts.specular_environment_id < 0) || (push.consts.specular_environment_id >= max_textures))
		return mix(getSky(R.xzy), (push.consts.top_sky_color.rgb + push.consts.bottom_sky_color.rgb) * .5, roughness);

    TextureStruct tex = txbo.textures[push.consts.specular_environment_id];
    
    float lod = roughness * max(tex.mip_levels - 4, 0.0);
    float lodf = floor(lod);
    float lodc = ceil(lod);
    
    vec3 a = textureLod(
        samplerCube(texture_cubes[push.consts.specular_environment_id], samplers[tex.sampler_id]), 
        R.xzy, lodf).rgb;

    vec3 b = textureLod(
        samplerCube(texture_cubes[push.consts.specular_environment_id], samplers[tex.sampler_id]), 
        R.xzy, lodc).rgb;

    return mix(a, b, lod - lodf);
}

// Analytical Lighting          ----------------------------------------------------
vec3 get_light_contribution(vec3 w_position, vec3 w_view, vec3 w_normal, vec3 albedo_mix, vec3 albedo, float metallic, float roughness)
{    
    /* Need a linear cosine transform lookup table for LTC... */
    if ((push.consts.ltc_mat_lut_id < 0) || (push.consts.ltc_mat_lut_id >= max_textures) ) return vec3(0.0);
    if ((push.consts.ltc_amp_lut_id < 0) || (push.consts.ltc_amp_lut_id >= max_textures)) return vec3(0.0);

    vec3 final_color = vec3(0.0);
    float dotNV = clamp(dot(w_normal, w_view), 0.0, 1.0);

    /* Get inverse linear cosine transform for area lights */
    float ndotv = saturate(dotNV);
    vec2 uv = vec2(roughness, sqrt(1.0 - ndotv));    
    uv = uv*LUT_SCALE + LUT_BIAS;
    vec4 t1 = texture(sampler2D(texture_2Ds[push.consts.ltc_mat_lut_id], samplers[0]), uv); // ltc_1
    // t1.w = max(t1.w, .05); // Causes numerical precision issues if too small
    vec4 t2 = texture(sampler2D(texture_2Ds[push.consts.ltc_amp_lut_id], samplers[0]), uv); // ltc_2
    mat3 m_inv = mat3(
        vec3(t1.x, 0, t1.y),
        vec3(  0,  1,    0),
        vec3(t1.z, 0, t1.w)
    );

    // F = Fresnel factor (Reflectance depending on angle of incidence)
    vec3 F = F_Schlick(dotNV, albedo_mix);

    /* Material diffuse influence (no albedo yet) */ // TODO, make frensel specific to point lights.
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);	

    /* For each light */
    for (int i = 0; i < max_lights; ++i) {
        /* Skip unused lights */
        int light_entity_id = lidbo.lightIDs[i];
        if (light_entity_id == -1) continue;
        
        /* Skip lights without a transform */
        EntityStruct light_entity = ebo.entities[light_entity_id];
        if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) continue;

        LightStruct light = lbo.lights[light_entity.light_id];

        /* If the drawn object is the light component (fake emission) */
        if (light_entity_id == push.consts.target_id) {
            final_color += light.color.rgb * light.intensity;
            continue;
        }
        
        TransformStruct light_transform = tbo.transforms[light_entity.transform_id];
        vec3 w_light_right =    light_transform.localToWorld[0].xyz;
        vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
        vec3 w_light_up =       light_transform.localToWorld[2].xyz;
        vec3 w_light_position = light_transform.localToWorld[3].xyz;
        vec3 w_light_dir = normalize(w_light_position - w_position);

        // Precalculate vectors and dot products	
        vec3 w_half = normalize(w_view + w_light_dir);
        float dotNH = clamp(dot(w_normal, w_half), 0.0, 1.0);
        float dotNL = clamp(dot(w_normal, w_light_dir), 0.0, 1.0);

        /* Some info for geometric terms */
        float dun = dot(normalize(w_light_up), w_normal);
        float drn = dot(normalize(w_light_right), w_normal);
        float dfn = dot(normalize(w_light_forward), w_normal);

        // D = Normal distribution (Distribution of the microfacets)
        float D = D_GGX(dotNH, roughness); 

        // G = Geometric shadowing term (Microfacets shadowing)
        float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);

        /* The color and intensity of the light */
        vec3 lcol = vec3(light.intensity * light.color.rgb);
        vec3 dcol = albedo * (1.0 - metallic);
        vec3 scol = albedo;

        bool double_sided = bool(light.flags & (1 << 0));
        bool show_end_caps = bool(light.flags & (1 << 1));
        
        float light_angle = 1.0 - (acos(abs( dot(normalize(w_light_up), w_light_dir))) / PI);
        float cone_angle_difference = clamp((light.coneAngle - light_angle) / ( max(light.coneAngle, .001) ), 0.0, 1.0);
        float cone_term = (light.coneAngle == 0.0) ? 1.0 : max((2.0 / (1.0 + exp( 6.0 * ((cone_angle_difference / max(light.coneSoftness, .01)) - 1.0)) )) - 1.0, 0.0);

        /* If casting shadows is disabled */
        float shadow_term = 1.0;
        if ((light.flags & (1 << 2)) != 0) {
            shadow_term = get_shadow_contribution(light_entity, light, w_light_position, w_position);
        }
        
        shadow_term *= cone_term;
        
        if (dotNL < 0.0) continue;
        
        float over_dist_squared = 1.0 / max(sqr(length(w_light_position - w_position)), 1.0);
        
        /* Point light */
        if ((light.flags & LIGHT_FLAGS_POINT) != 0) {
            vec3 spec = (D * F * G / (4.0 * dotNL * dotNV + 0.001));
            final_color += shadow_term * over_dist_squared * lcol * dotNL * (kD * albedo + spec);
        }
        
        /* Rectangle light */
        else if ((light.flags & LIGHT_FLAGS_PLANE) != 0)
        {
            /* Verify the area light isn't degenerate */
            if (distance(w_light_right, w_light_forward) < .01) continue;

            /* Compute geometric term */
            vec3 dr = w_light_right * drn + w_light_forward * dfn;
            float geometric_term = dot( normalize((w_light_position + dr) - w_position), w_normal);
            if (geometric_term < 0.0) continue;

            /* Create points for area light polygon */
            vec3 points[4];
            points[0] = w_light_position - w_light_right - w_light_forward;
            points[1] = w_light_position + w_light_right - w_light_forward;
            points[2] = w_light_position + w_light_right + w_light_forward;
            points[3] = w_light_position - w_light_right + w_light_forward;

            // Get Specular
            vec3 spec = LTC_Evaluate_Rect_Clipped(w_normal, w_view, w_position, m_inv, points, double_sided);
            
            // BRDF shadowing and Fresnel
            spec *= scol*t2.x + (1.0 - scol)*t2.y;

            // Get diffuse
            vec3 diff = LTC_Evaluate_Rect_Clipped(w_normal, w_view, w_position, mat3(1), points, double_sided); 

            final_color += shadow_term * geometric_term * lcol * (spec + dcol*diff);
        }

        /* Disk Light */
        else if ((light.flags & LIGHT_FLAGS_DISK) != 0)
        {
            /* Verify the area light isn't degenerate */
            if (distance(w_light_right, w_light_forward) < .1) continue;

            /* Compute geometric term */
            vec3 dr = w_light_right * drn + w_light_forward * dfn;
            float geometric_term = dot( normalize((w_light_position + dr) - w_position), w_normal);
            if (geometric_term < 0.0) continue;

            vec3 points[4];
            points[0] = w_light_position - w_light_right - w_light_forward;
            points[1] = w_light_position + w_light_right - w_light_forward;
            points[2] = w_light_position + w_light_right + w_light_forward;
            points[3] = w_light_position - w_light_right + w_light_forward;

            // Get Specular
            vec3 spec = LTC_Evaluate_Disk(w_normal, w_view, w_position, m_inv, points, double_sided);

            // BRDF shadowing and Fresnel
            spec *= scol*t2.x + (1.0 - scol)*t2.y;
            
            // Get diffuse
            vec3 diff = LTC_Evaluate_Disk(w_normal, w_view, w_position, mat3(1), points, double_sided); 

            final_color += shadow_term * geometric_term * lcol * (spec + dcol*diff);
        }
        
        /* Rod light */
        else if ((light.flags & LIGHT_FLAGS_ROD) != 0) {
            vec3 points[2];
            points[0] = w_light_position - w_light_up;
            points[1] = w_light_position + w_light_up;
            float radius = .1;//max(length(w_light_up), length(ex));

            /* Verify the area light isn't degenerate */
            if (length(w_light_up) < .1) continue;
            if (radius < .1) continue;

            /* Compute geometric term */
            vec3 dr = dun * w_light_up;
            float geometric_term = dot( normalize((w_light_position + dr) - w_position), w_normal);
            if (geometric_term <= 0) continue;
            
            // Get Specular
            vec3 spec = LTC_Evaluate_Rod(w_normal, w_view, w_position, m_inv, points, radius, show_end_caps);

            // BRDF shadowing and Fresnel
            spec *= scol*t2.x + (1.0 - scol)*t2.y;
            
            // Get diffuse
            vec3 diff = LTC_Evaluate_Rod(w_normal, w_view, w_position, mat3(1), points, radius, show_end_caps); 
            // diff /= (PI);

            final_color += shadow_term * geometric_term * lcol * (spec + dcol*diff);
        }
        
        /* Sphere light */
        else if ((light.flags & LIGHT_FLAGS_SPHERE) != 0)
        {
            /* construct orthonormal basis around L */
            vec3 T1, T2;
            generate_basis(w_light_dir, T1, T2);
            T1 = normalize(T1);
            T2 = normalize(T2);

            float s1 = length(w_light_right);
            float s2 = length(w_light_forward);
            float s3 = length(w_light_up);

            float maxs = max(max(abs(s1), abs(s2) ), abs(s3) );

            vec3 ex, ey;
            ex = T1 * maxs;
            ey = T2 * maxs;

            float temp = dot(ex, ey);
            if (temp < 0.0)
                ey *= -1;
            
            /* Verify the area light isn't degenerate */
            if (distance(ex, ey) < .01) continue;

            float geometric_term = dot( normalize((w_light_position + maxs * w_normal) - w_position), w_normal);
            if (geometric_term <= 0) continue;

            /* Create points for the area light around the light center */
            vec3 points[4];
            points[0] = w_light_position - ex - ey;
            points[1] = w_light_position + ex - ey;
            points[2] = w_light_position + ex + ey;
            points[3] = w_light_position - ex + ey;
            
            // Get Specular
            vec3 spec = LTC_Evaluate_Disk(w_normal, w_view, w_position, m_inv, points, true);

            // BRDF shadowing and Fresnel
            spec *= scol*t2.x + (1.0 - scol)*t2.y;

            // Get diffuse
            vec3 diff = LTC_Evaluate_Disk(w_normal, w_view, w_position, mat3(1), points, true); 

            final_color += shadow_term * geometric_term * lcol * (spec + dcol*diff);
        }
    }

    return final_color;
}

// Final Color Contribution Function          ----------------------------------------------------
// vec4 get_color_contribution(inout MaterialStruct material, vec3 N, vec3 V, vec3 P, vec3 MP, vec2 UV, vec4 VC)
// {
//     /* Compute reflection and refraction directions */
//     vec3 R = -normalize(reflect(V, N));	
// 	float eta = 1.0 / material.ior; // air = 1.0 / material = ior
// 	vec3 Refr = normalize(refract(-V, N, eta));

//     float metallic = material.metallic;
// 	float transmission = 1.0;//material.transmission;
// 	float roughness = getRoughness(material, UV, MP);
// 	float transmission_roughness = getTransmissionRoughness(material);
// 	vec4 albedo = getAlbedo(material, VC, UV, MP);
// 	float alphaMask = getAlphaMask(material, UV, MP);
//     float alpha = (alphaMask < 1.0) ? alphaMask : albedo.a; // todo: move alpha out of albedo. 
    
//     /* TODO: Add support for ray tracing */
// 	#ifndef RAYTRACING
//     if (alpha == 0.0) discard;
//     #endif

//     /* Todo: read from metallic/roughness texture if one exists. */
// 	vec3 reflection = getPrefilteredReflection(R, roughness).rgb;
// 	vec3 refraction = getPrefilteredReflection(Refr, min(transmission_roughness + roughness, 1.0) ).rgb;
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

//     // Ambient occlusion part
// 	float ao = /*(material.hasOcclusionTexture == 1.0f) ? texture(aoMap, inUV).r : */ 1.0f;
// 	vec3 ambient = (kD * diffuse + specular_reflection + specular_refraction) * ao;

//     // Iterate over point lights
// 	vec3 final_color = ambient + get_light_contribution(P, V, N, albedo_mix, albedo.rgb, metallic, roughness);

//     // Tone mapping
// 	final_color = Uncharted2Tonemap(final_color * push.consts.exposure);
// 	final_color = final_color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

//     // Gamma correction
// 	final_color = pow(final_color, vec3(1.0f / push.consts.gamma));

//     return vec4(final_color, alpha);
// }

struct PBRInfo
{
    int bounce_count;
    int entity_id;
    vec4 w_incoming;
    vec2 uv;
    vec4 m_normal;
    vec4 m_position;
};

// Contribution get_ray_traced_contribution(PBRInfo info)
// {
//     Contribution final_color;
//     final_color.diffuse_radiance = vec3(0.0);
//     final_color.specular_radiance = vec3(0.0);
//     final_color.alpha = 0.0;
    
//     /* Load target entity information */
//     EntityStruct target_entity = ebo.entities[info.entity_id];
//     if (target_entity.transform_id < 0 || target_entity.transform_id >= max_transforms) vec3(0.0);
//     TransformStruct target_transform = tbo.transforms[target_entity.transform_id];
//     if (target_entity.material_id < 0 || target_entity.material_id >= max_materials) vec3(0.0);
//     MaterialStruct target_material = mbo.materials[target_entity.material_id];

//     /* Hidden flag */
//     if ((target_material.flags & (1 << 1)) != 0) return final_color;

//     /* Compute some material properties */
//     const vec4 albedo = getAlbedo(target_material, vec4(0.0), info.uv, info.m_position.xyz);
//     const float metallic = clamp(target_material.metallic, 0.0, 1.0);
//     const float transmission = clamp(target_material.transmission, 0.0, 1.0);
//     const float roughness = getRoughness(target_material, info.uv, info.m_position.xyz);
//     const float clamped_roughness = clamp(roughness, 0.01, .99);
//     const float transmission_roughness = clamp(getTransmissionRoughness(target_material), 0.0, 0.9);

//     vec3 dielectric_specular = vec3(.04);
//     vec3 dielectric_diffuse = albedo.rgb;
//     vec3 metallic_transmissive_specular = albedo.rgb;
//     vec3 metallic_transmissive_diffuse = vec3(0.0);
// 	vec3 specular = mix(dielectric_specular, metallic_transmissive_specular, max(metallic, transmission));
//     vec3 diffuse = mix(dielectric_diffuse, metallic_transmissive_diffuse, max(metallic, transmission));
//     float KD = (1.0 - metallic);

//     float alphaMask = getAlphaMask(target_material, info.uv, info.m_position.xyz);
//     float alpha = (alphaMask < 1.0) ? alphaMask : albedo.a; // todo: move alpha out of albedo. 
    
//     /* TODO: Add support for ray tracing */
// 	if (alpha == 0.0)
//     #ifndef RAYTRACING
//     discard;
//     #else
//     return final_color;
//     #endif

//     /* Compute some common vectors and interpolants */
//     vec3 w_position = vec3(target_transform.localToWorld * vec4(info.m_position.xyz, 1.0));
//     vec3 w_normal = normalize((transpose(target_transform.worldToLocal) * vec4(info.m_normal.xyz, 0.0)).xyz);
//     vec3 w_incoming = info.w_incoming.xyz;
// 	if (dot(w_normal, info.w_incoming.xyz) >= 0.0) w_normal *= -1.0;    // This seems to mess up refractive surfaces
//     bool inside = (dot(w_normal, info.w_incoming.xyz) >= 0.0);
//     vec3 w_refr, w_refl;
//     // w_refl = normalize(reflect(w_incoming, w_normal));	
//     float eta = (inside) ? target_material.ior / 1.0 : 1.0 / target_material.ior;
//     w_refr = normalize(refract(w_incoming, w_normal, eta));
//     w_refl = normalize(reflect(w_incoming, w_normal));	
//     // if (inside) {
//     //     w_refr = normalize(refract(w_incoming, -w_normal, eta));
//     //     w_refl = normalize(reflect(w_incoming, -w_normal));	
//     // }
//     // else {
//     // }
//     // vec3 w_refl = normalize(reflect(w_incoming, w_normal));	
//     vec3 w_view = -info.w_incoming.xyz;
//     float NdotV = clamp(dot(w_normal, w_view), 0.0, 1.0);
// 	float schlick = pow(1.0 - NdotV, 5.0);

//     /* Compute direct specular and diffuse contribution from LTC area lights */
//     vec2 LTC_UV = vec2(clamped_roughness, sqrt(1.0 - NdotV))*LUT_SCALE + LUT_BIAS;
//     vec4 t1 = texture(sampler2D(texture_2Ds[push.consts.ltc_mat_lut_id], samplers[0]), LTC_UV);
//     vec4 t2 = texture(sampler2D(texture_2Ds[push.consts.ltc_amp_lut_id], samplers[0]), LTC_UV);
//     mat3 m_inv = mat3(
//         vec3(t1.x, 0, t1.y),
//         vec3(  0,  1,    0),
//         vec3(t1.z, 0, t1.w)
//     );

//     vec3 directDiffuseContribution = vec3(0.0);
//     vec3 directSpecularContribution = vec3(0.0);
//     vec3 emissiveContribution = vec3(0.0);

//     // int active_lights = 0; 
//     // for (int i = 0; i < max_lights; ++i) {
//     //     int light_entity_id = lidbo.lightIDs[i];
//     //     if (light_entity_id != -1) active_lights++;
//     // }

//     // #ifndef RAYTRACING
//     vec3 specular_irradiance = vec3(0.0);
//     vec3 diffuse_irradiance = vec3(0.0);
//     for (int i = 0; i < max_lights; ++i) {
//     // #else
//     // float rand0 = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(int(payload.pixel.x), int(payload.pixel.y), push.consts.frame, payload.bounce_count * 9);
//     // int selected = max(int(rand0 * active_lights), active_lights - 1);
//     // for (int i = selected; i < selected + 1; i++)
//     // {
//     // #endif
//         /* Skip unused lights */
//         int light_entity_id = lidbo.lightIDs[i];
//         if (light_entity_id == -1) continue;

//         /* Skip lights without a transform */
//         EntityStruct light_entity = ebo.entities[light_entity_id];
//         if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) continue;
//         LightStruct light = lbo.lights[light_entity.light_id];

//         /* If the drawn object is the light component (fake emission) */
//         if (light_entity_id == info.entity_id) {
//             emissiveContribution += light.color.rgb * light.intensity;
//             continue;
//         }

//         TransformStruct light_transform = tbo.transforms[light_entity.transform_id];
//         vec3 w_light_right =    light_transform.localToWorld[0].xyz;
//         vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
//         vec3 w_light_up =       light_transform.localToWorld[2].xyz;
//         vec3 w_light_position = light_transform.localToWorld[3].xyz;
//         vec3 w_light_dir = normalize(w_light_position - w_position);

//         // Precalculate vectors and dot products	
//         vec3 w_half = normalize(w_view + w_light_dir);
//         float dotNH = clamp(dot(w_normal, w_half), 0.0, 1.0);
//         float dotNL = clamp(dot(w_normal, w_light_dir), 0.0, 1.0);
//         if (dotNL < 0.0) continue;

//         /* Some info for geometric terms */
//         float dun = dot(normalize(w_light_up), w_normal);
//         float drn = dot(normalize(w_light_right), w_normal);
//         float dfn = dot(normalize(w_light_forward), w_normal);

//         vec3 lcol = vec3(light.intensity * light.color.rgb);
//         bool double_sided = bool(light.flags & (1 << 0));
//         bool show_end_caps = bool(light.flags & (1 << 1));

//         float light_angle = 1.0 - (acos(abs( dot(normalize(w_light_up), w_light_dir))) / PI);
//         float cone_angle_difference = clamp((light.coneAngle - light_angle) / ( max(light.coneAngle, .001) ), 0.0, 1.0);
//         float cone_term = (light.coneAngle == 0.0) ? 1.0 : max((2.0 / (1.0 + exp( 6.0 * ((cone_angle_difference / max(light.coneSoftness, .01)) - 1.0)) )) - 1.0, 0.0);

//         /* If casting shadows is disabled */
//         float shadow_term = 1.0;
//         if ((light.flags & (1 << 2)) != 0) {
//             #ifndef RAYTRACING
//             shadow_term = get_shadow_contribution(light_entity, light, w_light_position, w_position);
//             #else
//             if (info.bounce_count < 2) {

//             float dist = distance(w_light_position, w_position);

//             /* Trace a single shadow ray */
//             uint rayFlags = gl_RayFlagsNoneNV;
//             uint cullMask = 0xff;
//             float tmin = .01;
//             float tmax = dist + 1.0;
//             // uint rayFlags = gl_RayFlagsCullBackFacingTrianglesNV ;
//             vec3 dir = w_light_dir;

//             payload.is_shadow_ray = true; 
//             payload.random_dimension = randomDimension;
//             traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, w_position.xyz + w_normal * .01, tmin, dir, tmax, 0);
//             if ((payload.distance < 0.0) || (payload.entity_id < 0) || (payload.entity_id >= max_entities) || (payload.entity_id == light_entity_id)) 
//             {
//                 shadow_term = 1.0;
//             } else if (payload.distance < dist) {                
//                     shadow_term = 0.0;
//                 // final_color += (1.0 - roughness) * vec4(specular * payload.color.rgb, 0.0);
//             }
//             }
//             #endif
//         }

//         shadow_term *= cone_term;
        
//         float over_dist_squared = 1.0 / max(sqr(length(w_light_position - w_position)), 1.0);
        
//         /* Point light */
//         if (light.type == 0) {
//             // // D = Normal distribution (Distribution of the microfacets)
//             // float D = D_GGX(dotNH, roughness); 
//             // // G = Geometric shadowing term (Microfacets shadowing)
//             // float G = G_SchlicksmithGGX(dotNL, NdotV, roughness);
//             // // F = Fresnel factor (Reflectance depending on angle of incidence)
//             // vec3 F = F_Schlick(NdotV, F0);		
            
//             // vec3 spec = (D * F * G / (4.0 * dotNL * NdotV + 0.001));
//             // directDiffuseContribution += shadow_term * lcol * dotNL * KD * diffuse;
//             // directSpecularContribution += shadow_term * lcol * dotNL * spec;
//             // // final_color += shadow_term * over_dist_squared * lcol * dotNL * (kD * albedo + spec);
//         }
        
//         /* Rectangle light */
//         else if (light.type == 1)
//         {
//             /* Verify the area light isn't degenerate */
//             if (distance(w_light_right, w_light_forward) < .01) continue;

//             /* Compute geometric term */
//             vec3 dr = w_light_right * drn + w_light_forward * dfn;
//             float geometric_term = dot( normalize((w_light_position + dr) - w_position), w_normal);
//             if (geometric_term < 0.0) continue;

//             /* Create points for area light polygon */
//             vec3 points[4];
//             points[0] = w_light_position - w_light_right - w_light_forward;
//             points[1] = w_light_position + w_light_right - w_light_forward;
//             points[2] = w_light_position + w_light_right + w_light_forward;
//             points[3] = w_light_position - w_light_right + w_light_forward;

//             // Get Specular
//             vec3 lspec = LTC_Evaluate_Rect_Clipped(w_normal, w_view, w_position, m_inv, points, double_sided);
            
//             // BRDF shadowing and Fresnel
//             lspec *= specular*t2.x + (1.0 - specular)*t2.y;

//             // Get diffuse
//             vec3 ldiff = LTC_Evaluate_Rect_Clipped(w_normal, w_view, w_position, mat3(1), points, double_sided); 

//             /* Finding that the intensity is slightly more against ray traced ground truth */
//             float correction = 1.5 / PI;

//             diffuse_irradiance += shadow_term * lcol * lspec * correction;
//             specular_irradiance += shadow_term * lcol * ldiff * correction;
//             // final_color += shadow_term * geometric_term * lcol * (spec + diffuse*diff);
//         }

//         /* Disk Light */
//         else if (light.type == 2)
//         {
//             /* Verify the area light isn't degenerate */
//             if (distance(w_light_right, w_light_forward) < .1) continue;

//             /* Compute geometric term */
//             vec3 dr = w_light_right * drn + w_light_forward * dfn;
//             float geometric_term = dot( normalize((w_light_position + dr) - w_position), w_normal);
//             if (geometric_term < 0.0) continue;

//             vec3 points[4];
//             points[0] = w_light_position - w_light_right - w_light_forward;
//             points[1] = w_light_position + w_light_right - w_light_forward;
//             points[2] = w_light_position + w_light_right + w_light_forward;
//             points[3] = w_light_position - w_light_right + w_light_forward;

//             // Get Specular
//             vec3 lspec = LTC_Evaluate_Disk(w_normal, w_view, w_position, m_inv, points, double_sided);

//             // BRDF shadowing and Fresnel
//             lspec *= specular*t2.x + (1.0 - specular)*t2.y;
            
//             // Get diffuse
//             vec3 ldiff = LTC_Evaluate_Disk(w_normal, w_view, w_position, mat3(1), points, double_sided); 

//             diffuse_irradiance += shadow_term * lcol * lspec;
//             specular_irradiance += shadow_term * lcol * ldiff;
//             // final_color += shadow_term * geometric_term * lcol * (spec + diffuse*diff);
//         }
        
//         /* Rod light */
//         else if (light.type == 3) {
//             vec3 points[2];
//             points[0] = w_light_position - w_light_up;
//             points[1] = w_light_position + w_light_up;
//             float radius = .1;//max(length(w_light_up), length(ex));

//             /* Verify the area light isn't degenerate */
//             if (length(w_light_up) < .1) continue;
//             if (radius < .1) continue;

//             /* Compute geometric term */
//             vec3 dr = dun * w_light_up;
//             float geometric_term = dot( normalize((w_light_position + dr) - w_position), w_normal);
//             if (geometric_term <= 0) continue;
            
//             // Get Specular
//             vec3 lspec = LTC_Evaluate_Rod(w_normal, w_view, w_position, m_inv, points, radius, show_end_caps);

//             // BRDF shadowing and Fresnel
//             lspec *= specular*t2.x + (1.0 - specular)*t2.y;
            
//             // Get diffuse
//             vec3 ldiff = LTC_Evaluate_Rod(w_normal, w_view, w_position, mat3(1), points, radius, show_end_caps); 
//             // diff /= (PI);

//             diffuse_irradiance += shadow_term * lcol * lspec;
//             specular_irradiance += shadow_term * lcol * ldiff;
//             // final_color += shadow_term * geometric_term * lcol * (spec + diffuse*diff);
//         }
        
//         /* Sphere light */
//         else if (light.type == 4)
//         {
//             /* construct orthonormal basis around L */
//             vec3 T1, T2;
//             generate_basis(w_light_dir, T1, T2);
//             T1 = normalize(T1);
//             T2 = normalize(T2);

//             float s1 = length(w_light_right);
//             float s2 = length(w_light_forward);
//             float s3 = length(w_light_up);

//             float maxs = max(max(abs(s1), abs(s2) ), abs(s3) );

//             vec3 ex, ey;
//             ex = T1 * maxs;
//             ey = T2 * maxs;

//             float temp = dot(ex, ey);
//             if (temp < 0.0)
//                 ey *= -1;
            
//             /* Verify the area light isn't degenerate */
//             if (distance(ex, ey) < .01) continue;

//             float geometric_term = dot( normalize((w_light_position + maxs * w_normal) - w_position), w_normal);
//             if (geometric_term <= 0) continue;

//             /* Create points for the area light around the light center */
//             vec3 points[4];
//             points[0] = w_light_position - ex - ey;
//             points[1] = w_light_position + ex - ey;
//             points[2] = w_light_position + ex + ey;
//             points[3] = w_light_position - ex + ey;
            
//             // Get Specular
//             vec3 lspec = LTC_Evaluate_Disk(w_normal, w_view, w_position, m_inv, points, true);

//             // BRDF shadowing and Fresnel
//             lspec *= specular*t2.x + (1.0 - specular)*t2.y;

//             // Get diffuse
//             vec3 ldiff = LTC_Evaluate_Disk(w_normal, w_view, w_position, mat3(1), points, true); 

//             diffuse_irradiance += shadow_term * lcol * lspec;
//             specular_irradiance += shadow_term * lcol * ldiff;
//             // final_color += shadow_term * geometric_term * lcol * (spec + diffuse*diff);
//         }
//     }


//     directDiffuseContribution += (KD * diffuse) * diffuse_irradiance;
//     directSpecularContribution += (specular + diffuse) * specular_irradiance;

//     /* Compute indirect diffuse contribution from IBL */
//     vec3 ambient_diffuse_irradiance = sampleIrradiance(w_normal);
//     vec3 indirectDiffuse = diffuse * ambient_diffuse_irradiance;
//     vec3 indirectDiffuseContribution = KD * indirectDiffuse;

//     /* Compute indirect specular contribution from IBL */
//     float KRefr = min(transmission, (1.0 - metallic));
//     vec3 specular_reflective_irradiance = getPrefilteredReflection(w_refl, roughness);
//     vec3 specular_refractive_irradiance = getPrefilteredReflection(w_refr, max(transmission_roughness , roughness) );
//     vec2 specular_brdf = sampleBRDF(w_normal, w_view, roughness);
//     float reflective_schlick_influence = (mix(schlick, 1.0, metallic));
//     float refractive_schlick_influence =  1.0 - reflective_schlick_influence;
// 	vec3 indirectSpecularReflectionContribution = reflective_schlick_influence * specular_reflective_irradiance * (specular * specular_brdf.x + specular_brdf.y);
// 	vec3 indirectSpecularRefractionContribution = KRefr * refractive_schlick_influence * specular_refractive_irradiance * specular;


//     int bounce_count = info.bounce_count;

//     #ifdef RAYTRACING
//     float rand1 = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(int(payload.pixel.x), int(payload.pixel.y), push.consts.frame, 10);

//     // #define USE_BLUENOISE 
//     #ifdef USE_BLUENOISE
//     float theta = PI*samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(int(payload.pixel.x), int(payload.pixel.y), push.consts.frame, 15);
//     float phi = TWOPI*samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(int(payload.pixel.x), int(payload.pixel.y), push.consts.frame, 15);
//     #elif USE_HALTON
//     float theta = PI*Halton(2, int(payload.pixel.x), int(payload.pixel.y), push.consts.frame);
//     float phi = TWOPI*Halton(3, int(payload.pixel.x), int(payload.pixel.y), push.consts.frame); 
//     #else
//     float theta = PI * Hash(int(payload.pixel.x), int(payload.pixel.y), push.consts.frame, 0);
//     float phi = TWOPI * Hash(int(payload.pixel.x), int(payload.pixel.y), push.consts.frame, 1);
//     #endif
//     // float theta = TWOPI * 
//     vec3 ddir = normalize(vec3(sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta)));
//     vec3 rough_normal = normalize(w_normal + (ddir * roughness));

//     /* Ray trace reflections (glossy surfaces not yet supported) */
//     if ((bounce_count < 2) /*(roughness < rand1)*/) {
        
//         uint rayFlags = gl_RayFlagsNoneNV;
//         uint cullMask = 0xff;
//         float tmin = .1;
//         float tmax = 100.0;
//         // uint rayFlags = gl_RayFlagsCullBackFacingTrianglesNV ;
//         vec3 dir = normalize(reflect(w_incoming, rough_normal));

//         payload.bounce_count = bounce_count + 1;
//         payload.contribution.diffuse_radiance = payload.contribution.specular_radiance = vec3(0.0);
//         payload.contribution.alpha = 0.0;
//         payload.is_shadow_ray = false;
//         payload.random_dimension = randomDimension;
//         traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, w_position.xyz/*  + w_normal * .01*/, tmin, dir, tmax, 0);
//         if ((payload.distance < 0.0) || (payload.entity_id < 0) || (payload.entity_id >= max_entities)) {
//             final_color.specular_radiance += indirectSpecularReflectionContribution;
//         } 
//         else {
//             final_color.specular_radiance += specular * (payload.contribution.diffuse_radiance + payload.contribution.specular_radiance); 
//         }
//     } else
//     #endif
//     {
//         final_color.specular_radiance += indirectSpecularReflectionContribution;
//     }

//     /* Ray trace refractions (glossy surfaces not yet supported) */
//     #ifdef RAYTRACING
//     float rand2 = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(int(payload.pixel.x), int(payload.pixel.y), push.consts.frame, 11);
//     if ((bounce_count < 2) && (transmission > rand2)) {
//         vec3 absorption = vec3(0.0);//clamp(vec3(1.0) - albedo.rgb, vec3(0.0), vec3(1.0)) * 2.0;

//         uint rayFlags = gl_RayFlagsNoneNV;
//         uint cullMask = 0xff;
//         float tmin = .01;
//         float tmax = 100.0;
//         int bounce_count = info.bounce_count;

//         // uint rayFlags = gl_RayFlagsCullBackFacingTrianglesNV ;
//         vec3 dir = w_refr.xyz;

//         payload.bounce_count = bounce_count + 1;
//         payload.contribution.diffuse_radiance = payload.contribution.specular_radiance = vec3(0.0);
//         payload.contribution.alpha = 0.0;
//         payload.is_shadow_ray = false;
//         payload.random_dimension = randomDimension;
//         // float sign = (inside) ? -1.0 : 1.0;
//         traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, w_position.xyz/*  + w_normal * .1*/, tmin, dir, tmax, 0);
//         if ((payload.distance < 0.0) || (payload.entity_id < 0) || (payload.entity_id >= max_entities)) {
//             final_color.specular_radiance += indirectSpecularRefractionContribution;
//         } 
//         else {
//             vec3 absorped;
//             absorped.r = pow(E, -payload.distance * absorption.r);
//             absorped.g = pow(E, -payload.distance * absorption.g);
//             absorped.b = pow(E, -payload.distance * absorption.b);

//             final_color.specular_radiance += absorped * specular * (payload.contribution.diffuse_radiance + payload.contribution.specular_radiance);
//         }
//     } else
//     #endif
//     {
//         final_color.specular_radiance +=  indirectSpecularRefractionContribution;
//     }


//     final_color.diffuse_radiance += indirectDiffuseContribution;
//     final_color.specular_radiance += directSpecularContribution;
//     final_color.diffuse_radiance += directDiffuseContribution;
//     final_color.diffuse_radiance += emissiveContribution;


//     // final_color = vec4(w_normal, 1.0);
//     final_color.alpha = alpha;
//     return final_color;
// }




/* DISNEY BRDF */


void directionOfAnisotropicity(vec3 normal, out vec3 tangent, out vec3 binormal){
    tangent = cross(normal, vec3(1.,0.,1.));
    binormal = normalize(cross(normal, tangent));
    tangent = normalize(cross(normal,binormal));
}

// float visibilityTest(int light_entity_id, const in SurfaceInteraction interaction, vec3 ro, vec3 rd) {
//     #ifndef RAYTRACING
//     return 1.0;
//     #else
//     // SurfaceInteraction interaction = rayMarch(ro, rd);
//     // return IS_SAME_MATERIAL(interaction.objId, LIGHT_ID) ? 1. : 0.;

//     if (interaction.bounce_count <= push.consts.parameter1) {
//         float dist = INFINITY;

//         /* Trace a single shadow ray */
//         uint rayFlags = gl_RayFlagsNoneNV;
//         uint cullMask = 0xff;
//         float tmin = .02;
//         float tmax = dist + 1.0;
//         // uint rayFlags = gl_RayFlagsCullBackFacingTrianglesNV ;        
//         payload.is_shadow_ray = true; 
//         payload.random_dimension = randomDimension;
//         traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, ro, tmin, rd, tmax, 0); 
//         return ((payload.entity_id == light_entity_id) || (payload.entity_id == -1)) ? 1.0 : 0.0;
//     }


//     #endif

//     return 1.0;
// }

// vec3 sampleLightType( int light_entity_id, const in EntityStruct light_entity, const in SurfaceInteraction interaction, out vec3 wi, out float lightPdf, out float visibility/*, float seed*/) {
//     /* If the light is disabled */
//     // if( (light.flags & (1 << 2)) != 0 )
//         // return vec3(0.);

//     LightStruct light = lbo.lights[light_entity.light_id];
    
//     if (light.type == LIGHT_TYPE_POINT) {
//         vec3 L = pointLightSample(light_entity, interaction, wi, lightPdf/* , seed*/);
//         visibility = visibilityTest(light_entity_id, interaction, interaction.w_p.xyz + wi * .01, wi);
//         return L;
//     }
//     else if( light.type == LIGHT_TYPE_RECTANGLE ) {
//         vec3 L = rectangleLightSample(light_entity, interaction, wi, lightPdf/* , seed*/);
//         visibility = visibilityTest(light_entity_id, interaction, interaction.w_p.xyz + wi * .01, wi);
//         return L;
//     }
//     else if (light.type == LIGHT_TYPE_DISK) {
//         vec3 L = sphereLightSample(light_entity, interaction, wi, lightPdf/* , seed*/);
//         visibility = visibilityTest(light_entity_id, interaction, interaction.w_p.xyz + wi * .01, wi);
//         return L;
//     }
//     else if (light.type == LIGHT_TYPE_ROD) {
//         vec3 L = sphereLightSample(light_entity, interaction, wi, lightPdf/* , seed*/);
//         visibility = visibilityTest(light_entity_id, interaction, interaction.w_p.xyz + wi * .01, wi);
//         return L;
//     }
//     else if( light.type == LIGHT_TYPE_SPHERE ) {
//         vec3 L = sphereLightSample(light_entity, interaction, wi, lightPdf/* , seed*/);
//         visibility = visibilityTest(light_entity_id, interaction, interaction.w_p.xyz + wi * .01, wi);
//         return L;
//     } 
//     // else if( light.type == LIGHT_TYPE_SUN ) {
//     //     vec3 L = sampleSun(light, interaction, wi, lightPdf, seed);
//     //     visibility = visibilityTestSun(interaction.point + wi * .01, wi);
//     //     return L;
//     // }
//     // else {
//     //     return vec3(0.);
//     // }
// }


// vec3 calculateRadiance(int light_entity_id, const in EntityStruct light_entity, const in SurfaceInteraction interaction, const in MaterialStruct material) {
//     // Light MIS
//     vec3 wo = -interaction.w_incoming.xyz;
//     vec3 Ld = vec3(0.);
//     float lightPdf = 0., visibility = 1.;
//     bool isBlack = false;
//     LightStruct light = lbo.lights[light_entity.light_id];

//     vec3 wi;

//     /* If the drawn object has a light component, radiate the light */
//     EntityStruct interaction_entity = ebo.entities[interaction.entity_id];
//     vec3 emission = vec3(0.0);
//     if ((interaction_entity.light_id >= 0) && (interaction_entity.light_id < max_lights)) {
//         LightStruct light = lbo.lights[interaction_entity.light_id];
//         emission = light.color.rgb * light.intensity;
//         if (interaction.entity_id == light_entity_id) return emission;
//         if (interaction.bounce_count == 0) return emission;
//     }

//     /* Compute differential irradiance for this direct light */
//     vec3 Li = sampleLightType( light_entity_id, light_entity, interaction, wi, lightPdf, visibility/*, seed*/ );
//     Li *= visibility;
    
//     /* Compute outgoing radiance from the light */
//     isBlack = dot(Li, Li) <= 0.;
//     float light_scattering_pdf = 1.;
//     float light_weight = 1.0;
//     if (lightPdf > EPSILON && !isBlack ) {
//         vec3 f = bsdfEvaluate(wi, wo, interaction, material) * abs(dot(wi, interaction.w_n.xyz));
        
//         light_scattering_pdf = bsdfPdf(wi, wo, interaction, material);
//         light_weight = powerHeuristic(1., lightPdf, 1., light_scattering_pdf);

//         isBlack = dot(f, f) <= 0.;
//         if (!isBlack) {
//            Ld += Li * f * (light_weight / lightPdf);
//         }
//     }

//     /* Compute outgoing radiance by sampling the BRDF */
//     #ifdef RAYTRACING
//     float brdf_scattering_pdf = 1.0;
//     float brdf_weight = 1.0;
//     bool is_specular;
//     if (interaction.bounce_count < MAX_BOUNCES) {
//         /* Compute a random reflection direction */
//         vec2 u = vec2(random(), random());
//         vec3 f = bsdfSample( wi, wo, brdf_scattering_pdf, is_specular, interaction, material);
//         f *= abs(dot(wi, interaction.w_n.xyz));
//         isBlack = dot(f, f) <= 0.;

//         if (!isBlack && (brdf_scattering_pdf > EPSILON)) {
//             // /* TODO: Account for different light types */
//             // if( light.type == LIGHT_TYPE_SPHERE ) {
//             //     lightPdf = sphereLightPDF(light_entity, interaction);
//             // } else {
//             //     lightPdf = rectangleLightPDF(light_entity, interaction);
//             // }
//             if (lightPdf < EPSILON) return Ld;
//             brdf_weight = powerHeuristic(1., brdf_scattering_pdf, 1., lightPdf);
        
//             /* Compute differential irradiance */
//             float tmin = .1;
//             float tmax = 100;
//             uint rayFlags = gl_RayFlagsNoneNV;
//             uint cullMask = 0xff;
//             payload.is_shadow_ray = false; 
//             payload.bounce_count = interaction.bounce_count + 1;
//             traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, interaction.w_p.xyz, tmin, wi, tmax, 0);
//             Li = (payload.contribution.diffuse_radiance + payload.contribution.specular_radiance);

//             if (payload.entity_id == -1) brdf_weight = 1.0;

// //             /* If we didn't hit the light */
// //             if (!payload.contribution.is_direct_light) {
// //                 Ld *= (lightPdf / light_weight); // Undo MIS
// //                 brdf_scattering_pdf = 1.0;
// //                 weight = 1.0;
// //             }

// //             // Li *= visibilityTest(light_entity_id, interaction, interaction.w_p.xyz + wi * .01, wi);
//             isBlack = dot(Li, Li) <= 0.;
//         	if (!isBlack) {
//             	Ld += Li * f * (brdf_weight / brdf_scattering_pdf);
//             }

//         }
//     }
//     #endif

//     // // //     contribution.alpha = material.base_color.a;
// //     // //     beta = interaction.beta;
// //     //     if( brdf_scattering_pdf > EPSILON && dot(f, f) > EPSILON)
// //     //         f = f / brdf_scattering_pdf;


//     return Ld;
// }

// Contribution get_disney_ray_traced_contribution(MaterialStruct material, SurfaceInteraction interaction, out vec3 wi, out vec3 beta)
// {
//     randomDimension = interaction.random_dimension;

//     Contribution contribution;
//     contribution.diffuse_radiance = contribution.specular_radiance = vec3(0.0);
//     contribution.alpha = 1.0;
    
//     EntityStruct interaction_entity = ebo.entities[interaction.entity_id];
//     if (interaction_entity.transform_id < 0 || interaction_entity.transform_id >= max_transforms) return contribution;

//     /* If the source object has a light component, radiate the light, but only if primary visibility or immediately following a specular event */
//     vec3 emission = vec3(0.0);
//     if (
//         ((interaction_entity.light_id >= 0) && (interaction_entity.light_id < max_lights)) && 
//         ((interaction.is_specular) || (interaction.bounce_count == 0))
//     ) {
//         LightStruct light = lbo.lights[interaction_entity.light_id];
//         emission = light.color.rgb * light.intensity;
//         contribution.diffuse_radiance += emission;
//         return contribution;
//     }

//     TransformStruct transform = tbo.transforms[interaction_entity.transform_id];
//     EntityStruct light_entity;
//     LightStruct light;
//     TransformStruct light_transform;
//     int light_entity_id;

//     vec3 f = vec3(0.); 

//     /* Pick a random light for next event estimation */
//     bool light_found = false;
//     while (!light_found) {
//         int i = int(random(interaction.pixel_coords) * (push.consts.num_lights + .5));

//         /* Skip unused lights and lights without a transform */
//         light_entity_id = lidbo.lightIDs[i];
//         if (light_entity_id == -1) continue;
//         // if (light_entity_id == interaction.entity_id) break;
//         light_entity = ebo.entities[light_entity_id];
//         if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) continue;
//         light_found = true;
//         light = lbo.lights[light_entity.light_id];
//         light_transform = tbo.transforms[light_entity.transform_id];
//     }

//     /* If we didn't find a light, return */
//     if (!light_found) return contribution;

//     /* Compute the outgoing radiance for this light */
//     vec3 wo = -interaction.w_incoming.xyz;
//     vec3 Ld = vec3(0.);
//     float lightPdf = 0., visibility = 1.;
//     bool isBlack = false;

//     /* Compute differential irradiance and visibility */
//     vec3 Li = vec3(0);   
//     if (light.type == LIGHT_TYPE_POINT) {
//         Li = pointLightSample(light_entity, light, light_transform, interaction, wi, lightPdf/* , seed*/);
//         visibility = visibilityTest(light_entity_id, interaction, interaction.w_p.xyz, wi);
//     }
//     else if( light.type == LIGHT_TYPE_RECTANGLE ) {
//         Li = rectangleLightSample(light_entity, light, light_transform, interaction, wi, lightPdf/* , seed*/);
//         visibility = visibilityTest(light_entity_id, interaction, interaction.w_p.xyz, wi);
//     }
//     else if (light.type == LIGHT_TYPE_DISK) {
//         Li = sphereLightSample(light_entity, light, light_transform, interaction, wi, lightPdf/* , seed*/);
//         visibility = visibilityTest(light_entity_id, interaction, interaction.w_p.xyz, wi);
//     }
//     else if (light.type == LIGHT_TYPE_ROD) {
//         Li = sphereLightSample(light_entity, light, light_transform, interaction, wi, lightPdf/* , seed*/);
//         visibility = visibilityTest(light_entity_id, interaction, interaction.w_p.xyz, wi);
//     }
//     else if( light.type == LIGHT_TYPE_SPHERE ) {
//         Li = sphereLightSample(light_entity, light, light_transform, interaction, wi, lightPdf/* , seed*/);
//         visibility = visibilityTest(light_entity_id, interaction, interaction.w_p.xyz, wi);
//     }
//     Li *= visibility;

//     /* Compute outgoing radiance from the light by sampling it directly */
//     isBlack = dot(Li, Li) <= 0.;
//     float light_scattering_pdf = 1.;
//     float light_weight = 1.0;
//     if (lightPdf > EPSILON && !isBlack ) {
//         vec3 f = bsdfEvaluate(wi, wo, interaction, material) * abs(dot(wi, interaction.w_n.xyz));
        
//         light_scattering_pdf = bsdfPdf(wi, wo, interaction, material);
//         light_weight = powerHeuristic(1., lightPdf, 1., light_scattering_pdf);

//         isBlack = dot(f, f) <= 0.;
//         if (!isBlack) {
//            Ld += Li * f * (light_weight / lightPdf);
//         }
//     }

//     /* Compute outgoing radiance from the light by sampling the BRDF */
//     if ((interaction.bounce_count < push.consts.parameter1) && (!interaction.is_specular)) {
//         float brdf_scattering_pdf = 1.0;
//         float brdf_weight = 1.0;
//         bool is_specular;
//         /* Compute a random reflection direction */
//         vec3 f = bsdfSample(random(interaction.pixel_coords), random(interaction.pixel_coords), random(interaction.pixel_coords), 
//             wi, wo, brdf_scattering_pdf, is_specular, interaction, material);
//         f *= abs(dot(wi, interaction.w_n.xyz));
//         isBlack = dot(f, f) <= 0.;
//         brdf_weight = powerHeuristic(1., brdf_scattering_pdf, 1., lightPdf);

//         if (!isBlack && (brdf_scattering_pdf > EPSILON) && (brdf_weight > EPSILON)) {
//             // /* TODO: Account for different light types */
//             // if( light.type == LIGHT_TYPE_SPHERE ) {
//             //     lightPdf = sphereLightPDF(light_entity, interaction);
//             // } else {
//             //     lightPdf = rectangleLightPDF(light_entity, interaction);
//             // }
//             // if (lightPdf < EPSILON) {
//             //     contribution.diffuse_radiance += Ld;
//             //     return contribution;
//             // };
        
//     #ifdef RAYTRACING
//             /* Compute differential irradiance */
//             float tmin = .1;
//             float tmax = 100;
//             uint rayFlags = gl_RayFlagsNoneNV;
//             uint cullMask = 0xff;
//             payload.is_shadow_ray = false; 
//             payload.bounce_count = interaction.bounce_count + 1;
//             payload.is_specular_ray = true;
//             payload.random_dimension = randomDimension;
//             traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, interaction.w_p.xyz, tmin, wi, tmax, 0);
//             Li = (payload.contribution.diffuse_radiance + payload.contribution.specular_radiance);

//             // if (payload.entity_id == -1) brdf_weight = 1.0;
//     #endif
//             isBlack = dot(Li, Li) <= 0.;
//         	if (!isBlack) {
//             	Ld += Li * f * (brdf_weight / brdf_scattering_pdf);
//             }
//         }
//     }

//     /* Compute radiance from global illumination */
//     if ((interaction.bounce_count < push.consts.parameter1) && (!interaction.is_specular)) {
//         float brdf_scattering_pdf = 1.0;
//         float brdf_weight = 1.0;
//         bool is_specular;
//         /* Compute a random reflection direction */
//         vec3 f = bsdfSample(random(interaction.pixel_coords), random(interaction.pixel_coords), random(interaction.pixel_coords), 
//             wi, wo, brdf_scattering_pdf, is_specular, interaction, material);
//         f *= abs(dot(wi, interaction.w_n.xyz));
//         isBlack = dot(f, f) <= 0.;

//         if (!isBlack && (brdf_scattering_pdf > EPSILON)) {
// //             // /* TODO: Account for different light types */
// //             // if( light.type == LIGHT_TYPE_SPHERE ) {
// //             //     lightPdf = sphereLightPDF(light_entity, interaction);
// //             // } else {
// //             //     lightPdf = rectangleLightPDF(light_entity, interaction);
// //             // }
// //             if (lightPdf < EPSILON) {
// //                 contribution.diffuse_radiance += Ld;
// //                 return contribution;
// //             };
//             brdf_weight = 1.0;//powerHeuristic(1., brdf_scattering_pdf, 1., lightPdf);//powerHeuristic(1., brdf_scattering_pdf, 1., lightPdf);
        
//     #ifdef RAYTRACING
//             /* Compute differential irradiance */
//             float tmin = .1;
//             float tmax = 100;
//             uint rayFlags = gl_RayFlagsNoneNV;
//             uint cullMask = 0xff;
//             payload.is_shadow_ray = false; 
//             payload.bounce_count = interaction.bounce_count + 1;
//             payload.is_specular_ray = false;
//             payload.random_dimension = randomDimension;
//             traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, interaction.w_p.xyz, tmin, wi, tmax, 0);
//             Li = (payload.contribution.diffuse_radiance + payload.contribution.specular_radiance);

//             /* Dont double count direcly sampled lights */
//             if (payload.entity_id == light_entity_id) brdf_weight = 0.0;
//     #endif

// //             /* If we didn't hit the light */
// //             if (!payload.contribution.is_direct_light) {
// //                 Ld *= (lightPdf / light_weight); // Undo MIS
// //                 brdf_scattering_pdf = 1.0;
// //                 weight = 1.0;
// //             }

// // //             // Li *= visibilityTest(light_entity_id, interaction, interaction.w_p.xyz + wi * .01, wi);
//             isBlack = dot(Li, Li) <= 0.;
//         	if (!isBlack) {
//             	Ld += Li * f * (brdf_weight / brdf_scattering_pdf);
//             }

//         }
//     }

//     contribution.diffuse_radiance = Ld;
//     contribution.random_dimension = randomDimension;
//     return contribution;
// }

#endif