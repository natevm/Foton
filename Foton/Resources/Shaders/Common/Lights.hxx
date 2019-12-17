#ifndef LIGHTS_HXX
#define LIGHTS_HXX

#include "Foton/Resources/Shaders/Common/LTCCommon.hxx"
#include "Foton/Resources/Shaders/Common/OffsetRay.hxx"

// vec3 pointLightSample(const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n, out vec3 w_i, out float lightPdf) {
//     vec3 w_light_position = light_transform.localToWorld[3].xyz;
//     vec3 w_light_dir = normalize(w_light_position - w_p);
    
//     w_i = w_light_dir;    
//     float sinThetaMax2 = 1.0 / distanceSq(w_light_position, w_p);
//     float cosThetaMax = sqrt(max(EPSILON, 1. - sinThetaMax2));
//     if (dot(w_i, w_n) > 0.) {
//         lightPdf = 1. / (TWO_PI * (1. - cosThetaMax));
//     }
    
// 	return light.color.rgb * light.intensity;
// }

// float pointLightPDF( const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n ) { 
//     vec3 w_light_right =    light_transform.localToWorld[0].xyz;
//     vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
//     vec3 w_light_up =       light_transform.localToWorld[2].xyz;
//     vec3 w_light_position = light_transform.localToWorld[3].xyz;
//     vec3 w_light_dir = normalize(w_light_position - w_p);
    
//     float s1 = length(w_light_right);
//     float s2 = length(w_light_forward);
//     float s3 = length(w_light_up);

//     float light_radius = max(max(abs(s1), abs(s2) ), abs(s3) );

//     float sinThetaMax2 = light_radius * light_radius / distanceSq(w_light_position, w_p);
//     float cosThetaMax = sqrt(max(EPSILON, 1. - sinThetaMax2));
//     return 1. / (TWO_PI * (1. - cosThetaMax)); 
// }

vec3 sphereLightSample(const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n, out vec3 w_i, out float lightPdf, out float dist) {
    vec3 w_light_right =    light_transform.localToWorld[0].xyz;
    vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
    vec3 w_light_up =       light_transform.localToWorld[2].xyz;
    vec3 w_light_position = light_transform.localToWorld[3].xyz;
    vec3 w_light_dir = normalize(w_light_position - w_p);
    
    float s1 = length(w_light_right);
    float s2 = length(w_light_forward);
    float s3 = length(w_light_up);

    float light_radius = max(max(abs(s1), abs(s2) ), abs(s3) );
    
    vec2 u = vec2(random(), random());
    
    vec3 tangent = vec3(0.), binormal = vec3(0.);
    createBasis(w_light_dir, tangent, binormal);
    
    float sinThetaMax2 = light_radius * light_radius / distanceSq(w_light_position, w_p);
    float cosThetaMax = sqrt(max(EPSILON, 1. - sinThetaMax2));
    w_i = uniformSampleCone(u, cosThetaMax, tangent, binormal, w_light_dir);
    
    vec3 to_pt = w_light_position - w_p;
    dist = length(to_pt);
    if (dot(w_i, w_n) > 0.) {
        // lightPdf = 1. / (TWO_PI * (1. - cosThetaMax));
        float dist_sqr = dot(to_pt, to_pt);
        lightPdf = dist_sqr / (TWO_PI * (1. - cosThetaMax)); 
        // lightPdf = 1.0 / (TWO_PI * (1. - cosThetaMax)); 
    }

    
	return light.color.rgb * light.intensity;
}

float sphereLightPDF( const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n ) { 
    vec3 w_light_right =    light_transform.localToWorld[0].xyz;
    vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
    vec3 w_light_up =       light_transform.localToWorld[2].xyz;
    vec3 w_light_position = light_transform.localToWorld[3].xyz;
    vec3 w_light_dir = normalize(w_light_position - w_p);
    
    float s1 = length(w_light_right);
    float s2 = length(w_light_forward);
    float s3 = length(w_light_up);

    float light_radius = max(max(abs(s1), abs(s2) ), abs(s3) );

    float sinThetaMax2 = light_radius * light_radius / distanceSq(w_light_position, w_p);
    float cosThetaMax = sqrt(max(EPSILON, 1. - sinThetaMax2));
    vec3 to_pt = w_light_position - w_p;
	float dist_sqr = dot(to_pt, to_pt);
	// float dist_sqr = 1.0;
    return dist_sqr / (TWO_PI * (1. - cosThetaMax)); 
}

vec3 rectangleLightSample(const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n, out vec3 w_i, out float lightPdf) {
    vec3 w_light_right =    light_transform.localToWorld[0].xyz;
    vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
    vec3 w_light_up =       light_transform.localToWorld[2].xyz;
    vec3 w_light_position = light_transform.localToWorld[3].xyz;
    vec3 w_light_dir = normalize(w_light_position - w_p);

    /* Compute a random point on a unit plane */
    vec4 m_light_position = vec4(random() * 2.0 - 1.0, random() * 2.0 - 1.0, 0.0, 1.0);

    /* Transform that point to where the light actually is */
    vec4 w_light_sample_position = light_transform.localToWorld * m_light_position;

    vec3 to_light = w_light_sample_position.xyz - w_p;
    w_i = normalize(to_light);

    float n_dot_w = dot(normalize(w_light_up.xyz), normalize(w_i));

    float width = length(w_light_right);
    float height = length(w_light_forward);


    float surface_area = width * height;
	vec3 to_pt = to_light;//p - dir;
	float dist_sqr = dot(to_pt, to_pt);
	// float n_dot_w = dot(normalize(w_light_up.xyz), wi);
	// if (n_dot_w < EPSILON) {
	// 	lightPdf = 0.f;
	// }
	lightPdf = dist_sqr / ( n_dot_w * surface_area);

	return light.color.rgb * light.intensity;
}

float rectangleLightPDF( const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n ) { 
    vec3 w_light_right =    light_transform.localToWorld[0].xyz;
    vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
    vec3 w_light_up =       light_transform.localToWorld[2].xyz;
    vec3 w_light_position = light_transform.localToWorld[3].xyz;
    vec3 to_light = w_light_position - w_p;
    vec3 w_light_dir = normalize(w_light_position - w_p);
    
    float n_dot_w = dot(normalize(w_light_up.xyz), w_light_dir);

    float width = length(w_light_right);
    float height = length(w_light_forward);

    float surface_area = width * height;
	vec3 to_pt = to_light;//p - dir;
	float dist_sqr = dot(to_pt, to_pt);
	// float n_dot_w = dot(normalize(w_light_up.xyz), wi);
	// if (n_dot_w < EPSILON) {
	// 	lightPdf = 0.f;
	// }
    return dist_sqr / ( n_dot_w * surface_area);
}








struct Irradiance {
    vec3 specular;
    vec3 diffuse;
};

#if defined(CAMERA_RAYTRACING) || defined(GLOBAL_RAYTRACING)

Irradiance sample_direct_light_stochastic (
    const in MaterialStruct mat, 
    bool backface, 
    bool force_diffuse, bool force_specular, bool force_perfect_reflection, 
    const in vec3 w_p, const in vec3 w_n, const in vec3 w_x, const in vec3 w_y, const in vec3 w_o, 
    inout int light_entity_id, 
    out float diffuse_visibility, 
    out float specular_visibility,
    out vec3 w_l, 
    out float l_distance
) {
    Irradiance irradiance;
    irradiance.specular = vec3(0.0);
    irradiance.diffuse = vec3(0.0);

    diffuse_visibility = specular_visibility = 0.0;
    w_l = vec3(0);
    l_distance = 0.f;

    vec3 w_n_f = (backface) ? -w_n : w_n;

    /* Pick a random light */
    EntityStruct light_entity; TransformStruct light_transform;
    LightStruct light; MaterialStruct light_material;
    bool light_found = false;
    int tries = 0;
    while ((!light_found) && (tries < 3) && (push.consts.num_lights > 0)) {
        tries++;
        int i = (light_entity_id == -1) ? int((random() - EPSILON) * (push.consts.num_lights)) : light_entity_id;
        light_entity_id = lidbo.lightIDs[i];
        if (light_entity_id == -1) continue;
        unpack_entity(light_entity_id, light_entity, light_transform, light_material, light);
        if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) continue;
        light_found = true;
    }

    /* If we didn't find a light, return. (Shouldn't happen except during initialization) */
    if (!light_found) return irradiance;

    // Importance sample the light directly
    /* Compute the outgoing radiance for this light */
    bool light_is_double_sided = bool(light.flags & (1 << 0));
    mat3 m_inv;
    vec3 w_i;

    float attenuation;

    /* Compute differential irradiance and visibility */
    vec3 direct_albedo = vec3(0.0);
    {
        float light_pdf = 0.;
        float light_visibility = 1.;
        vec3 Li = vec3(0);   
        float dist = 0;
        if ((light.flags & LIGHT_FLAGS_POINT) != 0) {
            Li = sphereLightSample(light_entity, light, light_transform, w_p, w_n_f, w_i, light_pdf, dist);
        }
        else if ((light.flags & LIGHT_FLAGS_PLANE) != 0) {
            Li = rectangleLightSample(light_entity, light, light_transform, w_p, w_n_f, w_i, light_pdf);
        }
        else if ((light.flags & LIGHT_FLAGS_DISK) != 0) {
            Li = sphereLightSample(light_entity, light, light_transform, w_p, w_n_f, w_i, light_pdf, dist);
        }
        else if ((light.flags & LIGHT_FLAGS_ROD) != 0) {
            Li = sphereLightSample(light_entity, light, light_transform, w_p, w_n_f, w_i, light_pdf, dist);
        }
        else if ((light.flags & LIGHT_FLAGS_SPHERE) != 0) {
            Li = sphereLightSample(light_entity, light, light_transform, w_p, w_n_f, w_i, light_pdf, dist);
        }

        l_distance = dist;
        attenuation = get_light_attenuation(light, dist);

        float bsdf_pdf, dpdf, spdf;
        disney_pdf(mat, backface, w_n, w_o, w_i, w_x, w_y, bsdf_pdf, dpdf, spdf);       

        float w = 0;
        float w_diffuse = 0;
        float w_specular = 0;
        vec3 bsdf, dbsdf, sbsdf;
        disney_bsdf(mat, backface, w_n, w_o, w_i, w_x, w_y, bsdf, dbsdf, sbsdf);
        // We need radiance before and after visibility.
        if (light_pdf >= EPSILON && bsdf_pdf >= EPSILON/* && light_visibility == 1.0*/) {
            w  = power_heuristic(1.f, light_pdf, 1.f, bsdf_pdf);
            w_diffuse  = power_heuristic(1.f, light_pdf, 1.f, dpdf);
            w_specular = power_heuristic(1.f, light_pdf, 1.f, spdf);
            vec3 diffuse_light_influence = Li * abs(dot(w_i, w_n_f)) * w_diffuse / light_pdf;
            vec3 specular_light_influence = Li * abs(dot(w_i, w_n_f)) * w_specular / light_pdf;            
            irradiance.diffuse += diffuse_light_influence;
            irradiance.specular += specular_light_influence;
            // radiance += bsdf * LightInfluence;
        }

        // NOTE, trace shadow ray at the bottom here to reduce continuation 
        // state
        
        /* Compute visibility by tracing a shadow ray */
        {
            vec3 w_p_off = offset_ray(w_p, w_n);
            
            /* Trace a single shadow ray */
            uint rayFlags = gl_RayFlagsNoneNV;
            uint cullMask = 0xff;
            float tmax = INFINITY;
            payload.is_shadow_ray = true; 
            traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, w_p_off, TMIN, w_i, tmax, 0); 
            light_visibility = (payload.entity_id == light_entity_id) ? 1.0 : 0.0;
        }
        diffuse_visibility += light_visibility * w_diffuse;
        specular_visibility += light_visibility * w_specular;
        w_l += Li * w;
    }

    // Sample the BRDF to compute a light sample as well
    vec3 bsdf_albedo = vec3(0.0);
	{
		vec3 w_i;
		float bsdf_pdf;
        float bsdf_visibility = 1.0;
        float dpdf, spdf;
		vec3 bsdf, dbsdf, sbsdf;
        sample_disney_bsdf(mat, backface, 
            w_n, w_o, w_x, w_y,
            force_diffuse, force_specular, force_perfect_reflection,
            w_i, bsdf_pdf, dpdf, spdf, bsdf, dbsdf, sbsdf);
		
		float light_dist;
		vec3 light_pos;
		if (bsdf_pdf >= EPSILON) {
			float light_pdf; // = quad_light_pdf(light, light_pos, w_p, w_i);
            vec3 Li;
            if ((light.flags & LIGHT_FLAGS_POINT) != 0) {
                light_pdf = sphereLightPDF(light_entity, light, light_transform, w_p, w_n_f);
            }
            else if ((light.flags & LIGHT_FLAGS_PLANE) != 0) {
                light_pdf = rectangleLightPDF(light_entity, light, light_transform, w_p, w_n_f);
            }
            else if ((light.flags & LIGHT_FLAGS_DISK) != 0) {
                light_pdf = sphereLightPDF(light_entity, light, light_transform, w_p, w_n_f);
            }
            else if ((light.flags & LIGHT_FLAGS_ROD) != 0) {
                light_pdf = sphereLightPDF(light_entity, light, light_transform, w_p, w_n_f);
            }
            else if ((light.flags & LIGHT_FLAGS_SPHERE) != 0) {
                light_pdf = sphereLightPDF(light_entity, light, light_transform, w_p, w_n_f);
            }
            Li = light.color.rgb * light.intensity;
            float w = 0.0;
            float w_diffuse = 0.0;
            float w_specular = 0.0;
			if (light_pdf >= EPSILON) {
				w = power_heuristic(1.f, bsdf_pdf, 1.f, light_pdf);
				w_diffuse = power_heuristic(1.f, dpdf, 1.f, light_pdf);
				w_specular = power_heuristic(1.f, spdf, 1.f, light_pdf);
                // We need radiance before and after visibility.
				// if (bsdf_visibility == 1.0) 
                {
                    vec3 diffuse_light_influence = Li * abs(dot(w_i, w_n_f)) * w_diffuse / dpdf;
                    vec3 specular_light_influence = Li * abs(dot(w_i, w_n_f)) * w_specular / spdf;
                    irradiance.diffuse += diffuse_light_influence;
                    irradiance.specular += specular_light_influence;
                    // radiance += bsdf * LightInfluence;
				}

                /* Compute visibility by tracing a shadow ray */
                {
                    vec3 w_p_off = offset_ray(w_p, w_n);

                    /* Trace a single shadow ray */
                    payload.is_shadow_ray = true; 
                    traceNV(topLevelAS, /*gl_RayFlagsTerminateOnFirstHitNV*/gl_RayFlagsNoneNV, 0xff, 0, 0, 0, w_p_off, TMIN, w_i, TMAX, 0); // UPDATE TMAX TO BE LIGHT DIST
                    bsdf_visibility = (payload.entity_id == light_entity_id) ? 1.0 : 0.0;
                }
                diffuse_visibility += bsdf_visibility * w_diffuse;
                specular_visibility += bsdf_visibility * w_specular;
                // visibility += bsdf_visibility * w;
                w_l += Li * w;
			}
		}        
	}
    w_l = normalize(w_l);
    
    return irradiance;
	// return radiance * attenuation;
}

#endif

Irradiance sample_direct_light_analytic(const in MaterialStruct mat, bool backface, bool include_shadows,  
    bool force_diffuse, bool force_specular, bool force_perfect_reflection,
    const in vec3 w_p, const in vec3 w_n, const in vec3 w_x, const in vec3 w_y, const in vec3 w_o) 
{
    float shadow_term = 1.0;

    vec3 specular_irradiance = vec3(0.0);
    vec3 diffuse_irradiance = vec3(0.0);

    Irradiance irradiance;
    irradiance.specular = vec3(0.0);
    irradiance.diffuse = vec3(0.0);

    vec3 w_n_f = (backface) ? -w_n : w_n;
    // analytical area lights become numerically unstable at extreme roughness
    const float clamped_roughness = clamp(mat.roughness, 0.001, .99);
    float NdotV = clamp(dot(w_n, w_o), 0.0, 1.0);
    vec3 specular = vec3(1.0) - min(vec3(clamped_roughness * clamped_roughness), 1.0);
    // vec3 diffuse = mat.base_color.rgb * (1.f - mat.metallic) * (1.f - mat.transmission);

    /* Pick a random light */
    EntityStruct light_entity; TransformStruct light_transform;
    LightStruct light; MaterialStruct light_material;
    
    // bool light_found = false;
    // int tries = 0;
    // while ((!light_found) && (tries < 3) && (push.consts.num_lights > 0)) {
    //     tries++;
    //     int i = int((random() - EPSILON) * (push.consts.num_lights));
    //     light_entity_id = lidbo.lightIDs[i];
    //     if (light_entity_id == -1) continue;
    //     unpack_entity(light_entity_id, light_entity, light_transform, light_material, light);
    //     if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) continue;
    //     light_found = true;
    // }
    

    int lights_hit = 0;
    for (int i = 0; i < push.consts.num_lights; ++i) {

        int light_entity_id = lidbo.lightIDs[i];
        if (light_entity_id == -1) continue;
        unpack_entity(light_entity_id, light_entity, light_transform, light_material, light);
        if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) continue;
        lights_hit+=1;

        vec3 lcol = vec3(light.intensity * light.color.rgb);
        bool double_sided = bool(light.flags & (1 << 0));
        bool show_end_caps = bool(light.flags & (1 << 1));

        /* Compute direct specular and diffuse contribution from LTC area lights */
        vec2 LTC_UV = vec2(clamped_roughness, sqrt(1.0 - NdotV))*LUT_SCALE + LUT_BIAS;
        vec4 t1 = texture(sampler2D(LTC_MAT, samplers[0]), LTC_UV);
        vec4 t2 = texture(sampler2D(LTC_AMP, samplers[0]), LTC_UV);
        mat3 m_inv = mat3(
            vec3(t1.x, 0, t1.y),
            vec3(  0,  1,    0),
            vec3(t1.z, 0, t1.w)
        );

        vec3 w_light_right =    light_transform.localToWorld[0].xyz;
        vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
        vec3 w_light_up =       light_transform.localToWorld[2].xyz;
        vec3 w_light_position = light_transform.localToWorld[3].xyz;
        vec3 w_light_dir = normalize(w_light_position - w_p);

        // Precalculate vectors and dot products	
        vec3 w_half = normalize(w_o + w_light_dir);
        float dotNH = clamp(dot(w_n, w_half), 0.0, 1.0);
        float dotNL = clamp(dot(w_n, w_light_dir), 0.0, 1.0);
        
        /* Some info for geometric terms */
        float dun = dot(normalize(w_light_up), w_n);
        float drn = dot(normalize(w_light_right), w_n);
        float dfn = dot(normalize(w_light_forward), w_n);

        /* Rectangle light */
        if ((light.flags & LIGHT_FLAGS_PLANE) != 0)
        {
            /* Verify the area light isn't degenerate */
            // if (distance(w_light_right, w_light_forward) < .01) continue;

            /* Compute geometric term */
            vec3 dr = w_light_right * drn + w_light_forward * dfn;
            // float geometric_term = dot( normalize((w_light_position + dr) - w_position), w_normal);
            // if (geometric_term < 0.0) continue;

            /* Create points for area light polygon */
            vec3 points[4];
            points[0] = w_light_position - w_light_right - w_light_forward;
            points[1] = w_light_position + w_light_right - w_light_forward;
            points[2] = w_light_position + w_light_right + w_light_forward;
            points[3] = w_light_position - w_light_right + w_light_forward;

            // Get Specular
            vec3 lspec = LTC_Evaluate_Rect_Clipped(w_n, w_o, w_p, m_inv, points, double_sided);
            
            // BRDF shadowing and Fresnel
            lspec *= specular*t2.x + (1.0 - specular)*t2.y;

            // Get diffuse
            vec3 ldiff = LTC_Evaluate_Rect_Clipped(w_n, w_o, w_p, mat3(1), points, double_sided); 

            /* Finding that the intensity is slightly more against ray traced ground truth */
            float correction = 1.5 / PI;

            diffuse_irradiance += shadow_term * lcol * ldiff * correction;
            specular_irradiance += shadow_term * lcol * lspec * correction;
            // final_color += shadow_term * geometric_term * lcol * (spec + diffuse*diff);
        }

        /* Disk Light */
        else if ((light.flags & LIGHT_FLAGS_DISK) != 0)
        {
            /* Verify the area light isn't degenerate */
            // if (distance(w_light_right, w_light_forward) < .1) continue;

            /* Compute geometric term */
            vec3 dr = w_light_right * drn + w_light_forward * dfn;
            // float geometric_term = dot( normalize((w_light_position + dr) - w_position), w_normal);
            // if (geometric_term < 0.0) continue;

            vec3 points[4];
            points[0] = w_light_position - w_light_right - w_light_forward;
            points[1] = w_light_position + w_light_right - w_light_forward;
            points[2] = w_light_position + w_light_right + w_light_forward;
            points[3] = w_light_position - w_light_right + w_light_forward;

            // Get Specular
            vec3 lspec = LTC_Evaluate_Disk(w_n, w_o, w_p, m_inv, points, double_sided);

            // BRDF shadowing and Fresnel
            lspec *= specular*t2.x + (1.0 - specular)*t2.y;
            
            // Get diffuse
            vec3 ldiff = LTC_Evaluate_Disk(w_n, w_o, w_p, mat3(1), points, double_sided); 

            diffuse_irradiance += shadow_term * lcol * ldiff;
            specular_irradiance += shadow_term * lcol * lspec;
            // final_color += shadow_term * geometric_term * lcol * (spec + diffuse*diff);
        }
        
        /* Rod light */
        else if ((light.flags & LIGHT_FLAGS_ROD) != 0) {
            vec3 points[2];
            points[0] = w_light_position - w_light_up;
            points[1] = w_light_position + w_light_up;
            float radius = .1;//max(length(w_light_up), length(ex));

            /* Verify the area light isn't degenerate */
            // if (length(w_light_up) < .1) continue;
            // if (radius < .1) continue;

            /* Compute geometric term */
            vec3 dr = dun * w_light_up;
            // float geometric_term = dot( normalize((w_light_position + dr) - w_position), w_normal);
            // if (geometric_term <= 0) continue;
            
            // Get Specular
            vec3 lspec = LTC_Evaluate_Rod(w_n, w_o, w_p, m_inv, points, radius, show_end_caps);

            // BRDF shadowing and Fresnel
            lspec *= specular*t2.x + (1.0 - specular)*t2.y;
            
            // Get diffuse
            vec3 ldiff = LTC_Evaluate_Rod(w_n, w_o, w_p, mat3(1), points, radius, show_end_caps); 
            // diff /= (PI);

            diffuse_irradiance += shadow_term * lcol * ldiff;
            specular_irradiance += shadow_term * lcol * lspec;
            // final_color += shadow_term * geometric_term * lcol * (spec + diffuse*diff);
        }
        
        // Sphere light / Point light
        else if (((light.flags & LIGHT_FLAGS_SPHERE) != 0) || ((light.flags & LIGHT_FLAGS_POINT) != 0)) {
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

            /* geometric term prevents artifacts when lighting faces from the back */
            float geometric_term = max(dot( normalize((w_light_position + maxs * w_n) - w_p), w_n), 0.0);
            if (geometric_term > 0.0) {
                /* Create points for the area light around the light center */
                vec3 points[4];
                points[0] = w_light_position - ex - ey;
                points[1] = w_light_position + ex - ey;
                points[2] = w_light_position + ex + ey;
                points[3] = w_light_position - ex + ey;
                
                // Get Specular
                vec3 lspec = LTC_Evaluate_Disk(w_n, w_o, w_p, m_inv, points, true);

                // BRDF shadowing and Fresnel
                lspec *= specular*t2.x + (1.0 - specular)*t2.y;

                // Get diffuse
                vec3 ldiff = LTC_Evaluate_Disk(w_n, w_o, w_p, mat3(1), points, true); 

                diffuse_irradiance += shadow_term * lcol * ldiff * geometric_term;
                specular_irradiance += shadow_term * lcol * lspec * geometric_term;
            }
        }
        

        Irradiance analytical_irradiance;
        analytical_irradiance.diffuse = diffuse_irradiance;
        analytical_irradiance.specular = specular_irradiance;
        
        float l_dist = distance(w_light_position, w_p); // approximation    
        float attenuation = get_light_attenuation(light, l_dist);

        if (((light.flags & LIGHT_FLAGS_CAST_SHADOWS) == 0) || (include_shadows == false)) {
            irradiance.diffuse += analytical_irradiance.diffuse * attenuation;
            irradiance.specular += analytical_irradiance.specular * attenuation;
            continue;
        }

        #if defined(CAMERA_RAYTRACING) || defined(GLOBAL_RAYTRACING)
        {
            // compute shadow
            float diffuse_visibility = 0.0;           
            float specular_visibility = 0.0;           
            vec3 w_l;
            Irradiance stochastic_differential_irradiance = sample_direct_light_stochastic(
                mat, backface, force_diffuse, force_specular, force_perfect_reflection,
                w_p, w_n, w_x, w_y, w_o, i, diffuse_visibility, specular_visibility,
                w_l, l_dist);
                
            // float stochastic_luminance = average_vec(stochastic_differential_radiance);//TWO_PI;

            // float geometric_term = dot( w_n, normalize(w_l));
            // if ((geometric_term <= 0) || (isnan(geometric_term))) return vec3(0.0);

            
            irradiance.diffuse += analytical_irradiance.diffuse * attenuation * diffuse_visibility;
            irradiance.specular += analytical_irradiance.specular * attenuation * specular_visibility;

            // irradiance.diffuse += stochastic_differential_irradiance.diffuse;// * diffuse_visibility * attenuation;
            // irradiance.specular += stochastic_differential_irradiance.specular;// * specular_visibility * attenuation;
        }
        #else
        {
            irradiance.diffuse += analytical_irradiance.diffuse * attenuation;
            irradiance.specular += analytical_irradiance.specular * attenuation;
        }
        
        #endif

        

        // if (stochastic_luminance > .0) {
        //     // float dist_sqrd = (l_dist*l_dist); // ideally this wouldn't be so noisy...
        //     // dist_sqrd = max(dist_sqrd, EPSILON);
        //     vec3 result = analytical_radiance/* * attenuation*/;// ((/*geometric_term**/analytical_radiance) / dist_sqrd);

        //     if (visibility < 1.0) {
        //         if (stochastic_luminance > EPSILON) {
        //             radiance += (result / stochastic_luminance) * visibility;
        //             continue;
        //         }
        //         else {
        //             radiance += result * visibility;
        //             continue;
        //         }
        //     }
        //     // return result * visibility;
        //     radiance += result;
        //     continue;
        // }
    }

    if (lights_hit == 0) {
        Irradiance empty;
        empty.diffuse = vec3(0.0);
        empty.specular = vec3(0.0);
        return empty;
    }

    irradiance.diffuse /= float(lights_hit);
    irradiance.specular /= float(lights_hit);

    return irradiance;
}

#endif