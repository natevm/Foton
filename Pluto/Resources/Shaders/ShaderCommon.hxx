#ifndef SHADER_COMMON
#define SHADER_COMMON

#include "Pluto/Resources/Shaders/LTCCommon.hxx"
#include "Pluto/Resources/Shaders/ShaderConstants.hxx"

// Tone Mapping --------------------------------------
// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return max(vec3(0.0f),  ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F );
}

// Texture Lookups --------------------------------------
// --- unfiltered checkerboard ---
float checkersTexture( in vec3 p )
{
    vec3 q = floor(p);
    return mod( q.x+q.y+q.z, 2.0 );            // xor pattern
}

// From GPU gems
float filterwidth(in vec3 v)
{
    #ifndef RAYTRACING
    vec3 fw = max(abs(dFdxFine(v)), abs(dFdyFine(v)));
    return max(max(fw.x, fw.y), fw.z);
    #else
    vec3 q = floor(v);
    return mod( q.x+q.y+q.z, 2.0 );            // xor pattern
    #endif
}

float checker(in vec3 uvw, float scale)
{
    float width = filterwidth(uvw*scale);
    vec3 p0 = (uvw * scale) - .5 * width;
    vec3 p1 = (uvw * scale) + .5 * width;
    #define BUMPINT(x) (floor((x)/2.0f) + 2.f * max( ((x)/2.0f ) - floor((x)/2.0f ) - .5f, 0.f))
    vec3 i = (BUMPINT(p1) - BUMPINT(p0)) / width;
    return i.x * i.y * i.z + (1.0f - i.x) * (1.0f - i.y) * (1.0f - i.z);
}

vec4 sample_texture_2D(vec4 default_color, int texture_id, vec2 uv, vec3 m_position)
{
    vec4 color = default_color; 
    if ((texture_id < 0) || (texture_id >= MAX_TEXTURES)) return default_color;

    TextureStruct tex = txbo.textures[texture_id];
    
    if ((tex.sampler_id < 0) || (tex.sampler_id >= MAX_SAMPLERS))  return default_color;

    /* Raster texture */
    if (tex.type == 0) {
        color = texture(sampler2D(texture_2Ds[texture_id], samplers[tex.sampler_id]), uv);
    }

    /* Procedural checker texture */
    else if (tex.type == 1)
    {
        float mate = checker(m_position, tex.scale);
        // If I do gamma correction here on linux, I get weird nans...
        color = mix(tex.color1, tex.color2, mate);
    }

    return color;
}

vec4 sample_texture_3D(vec4 default_color, int texture_id, vec3 uvw, float lod)
{
    if ((texture_id < 0) || (texture_id >= MAX_TEXTURES)) return default_color;
    vec4 color = default_color;

    TextureStruct tex = txbo.textures[texture_id];
    
    if ((tex.sampler_id < 0) || (tex.sampler_id >= MAX_SAMPLERS))  return default_color;

    if (tex.type == 0)
    {
        color = texture(sampler3D(texture_3Ds[texture_id], samplers[tex.sampler_id]), uvw);
    }
    else if (tex.type == 1)
    {
        float mate = checker(uvw, tex.scale);//texture(sampler3D(texture_3Ds[material.volume_texture_id], samplers[material.volume_texture_id]), normalize(position) );
        color = mix(tex.color1, tex.color2, mate);
    }

    //textureLod(volumeTexture, pos, mbo.lod);
    return color;//vec4(color.rgb, 0.0);
}

// Material Properties --------------------------------------
vec4 getAlbedo(inout MaterialStruct material, vec4 vert_color, vec2 uv, vec3 m_position)
{
	vec4 albedo = material.base_color; 

	/* If the use vertex colors flag is set, use the vertex color as the base color instead. */
	if ((material.flags & (1 << 0)) != 0)
		albedo = vert_color;

    albedo = sample_texture_2D(albedo, material.base_color_texture_id, uv, m_position);
	return vec4(pow(albedo.rgb, vec3(push.consts.gamma)), albedo.a);
}

float getAlphaMask(inout MaterialStruct material, vec2 uv, vec3 m_position) {
    return sample_texture_2D(vec4(1.0), material.alpha_texture_id, uv, m_position).r;
}

float getRoughness(inout MaterialStruct material, vec2 uv, vec3 m_position)
{
    float roughness = sample_texture_2D(vec4(material.roughness), material.roughness_texture_id, uv, m_position).r;
    return clamp(roughness, 0.0, 1.0);
}

float getTransmissionRoughness(inout MaterialStruct material)
{
    return clamp(material.transmission_roughness, 0.0, 1.0);
}

// Todo: add support for ray tracing (dfd... not supported)
// See http://www.thetenthplanet.de/archives/1180
vec3 perturbNormal(inout MaterialStruct material, vec3 w_position, vec3 w_normal, vec2 uv, vec3 m_position)
{
    #ifndef RAYTRACING
    w_normal = normalize(w_normal);
	vec3 tangentNormal = sample_texture_2D(vec4(w_normal.xyz, 0.0), material.bump_texture_id, uv, m_position).xyz;// texture(normalMap, inUV).xyz * 2.0 - 1.0;
    if (w_normal == tangentNormal) return w_normal;

    // tangentNormal = normalize(  tangentNormalDX + tangentNormalDY + vec3(0.0, 0.0, 1.0) );//vec3(0.0, 0.0, 1.0);

	vec3 q1 = dFdx(w_position);
	vec3 q2 = dFdy(w_position);
	vec2 st1 = dFdx(uv);
	vec2 st2 = dFdy(uv);

    // vec3 T = vec3(1.0, 0.0, 0.0); // Right
    // vec3 B = vec3(0.0, 1.0, 0.0); // Forward
    // vec3 N = vec3(0.0, 0.0, 1.0); // Up
	
    vec3 N = normalize(w_normal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

    EntityStruct target_entity = ebo.entities[push.consts.target_id];
    TransformStruct target_transform = tbo.transforms[target_entity.transform_id];
    // tangentNormal = (transpose(target_transform.worldToLocal) * vec4(w_normal, 0.0)).xyz;

	return normalize(TBN * tangentNormal);
    #else
    return w_normal;
    #endif
	// return tangentNormal;
}

// BRDF Lookup Tables --------------------------------------
vec2 sampleBRDF(vec3 N, vec3 V, float roughness)
{
    if ((push.consts.brdf_lut_id < 0) || (push.consts.brdf_lut_id >= MAX_TEXTURES))
        return vec2(0.0, 0.0);

    TextureStruct tex = txbo.textures[push.consts.brdf_lut_id];

    if ((tex.sampler_id < 0) || (tex.sampler_id >= MAX_SAMPLERS)) 
        return vec2(0.0, 0.0);

	return texture( 
			sampler2D(texture_2Ds[push.consts.brdf_lut_id], samplers[tex.sampler_id]), 
			vec2(max(dot(N, V), 0.0), roughness)
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
    if ((tex.sampler_id < 0) || (tex.sampler_id >= MAX_TEXTURES)) return 1.0;

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
	
	if (push.consts.environment_id != -1) {
		TextureStruct tex = txbo.textures[push.consts.environment_id];

		float lod = push.consts.environment_roughness * max(tex.mip_levels, 0.0);
		float lodf = floor(lod);
		float lodc = ceil(lod);
		
		vec3 a = textureLod(
			samplerCube(texture_cubes[push.consts.environment_id], samplers[tex.sampler_id]), 
			dir, lodf).rgb;

		vec3 b = textureLod(
			samplerCube(texture_cubes[push.consts.environment_id], samplers[tex.sampler_id]), 
			dir, lodc).rgb;

		return mix(a, b, lod - lodf);
	}

    return getSky(dir);


	// vec3 up = vec3(0.0, 1.0, 0.0);
	// float alpha = dot(up, dir);
	// return vec3(alpha, alpha, alpha);
}

vec3 sampleIrradiance(vec3 N)
{
    if ((push.consts.diffuse_environment_id < 0 ) || (push.consts.diffuse_environment_id >= MAX_TEXTURES))
	    return getSky(N.xzy);

    TextureStruct tex = txbo.textures[push.consts.diffuse_environment_id];
    
    if ((tex.sampler_id < 0 ) || (tex.sampler_id >= MAX_SAMPLERS))
        return getSky(N.xzy);

    return texture(
        samplerCube(texture_cubes[push.consts.diffuse_environment_id], samplers[tex.sampler_id]), N.xzy
    ).rgb;
}

vec3 getPrefilteredReflection(vec3 R, float roughness)
{
	if ((push.consts.specular_environment_id < 0) || (push.consts.specular_environment_id >= MAX_TEXTURES))
		return mix(getSky(R.xzy), (push.consts.top_sky_color.rgb + push.consts.bottom_sky_color.rgb) * .5, roughness);

    TextureStruct tex = txbo.textures[push.consts.specular_environment_id];
    
    float lod = roughness * max(tex.mip_levels, 0.0);
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
    if ((push.consts.ltc_mat_lut_id < 0) || (push.consts.ltc_mat_lut_id >= MAX_TEXTURES) ) return vec3(0.0);
    if ((push.consts.ltc_amp_lut_id < 0) || (push.consts.ltc_amp_lut_id >= MAX_TEXTURES)) return vec3(0.0);

    vec3 finalColor = vec3(0.0);
    float dotNV = clamp(dot(w_normal, w_view), 0.0, 1.0);

    /* Get inverse linear cosine transform for area lights */
    float ndotv = saturate(dotNV);
    vec2 uv = vec2(roughness, sqrt(1.0 - ndotv));    
    uv = uv*LUT_SCALE + LUT_BIAS;
    vec4 t1 = texture(sampler2D(texture_2Ds[push.consts.ltc_mat_lut_id], samplers[0]), uv);
    // t1.w = max(t1.w, .05); // Causes numerical precision issues if too small
    vec4 t2 = texture(sampler2D(texture_2Ds[push.consts.ltc_amp_lut_id], samplers[0]), uv);
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
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        /* Skip unused lights */
        int light_entity_id = push.consts.light_entity_ids[i];
        if (light_entity_id == -1) continue;
        
        /* Skip lights without a transform */
        EntityStruct light_entity = ebo.entities[light_entity_id];
        if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) continue;

        LightStruct light = lbo.lights[light_entity.light_id];

        /* If the drawn object is the light component (fake emission) */
        if (light_entity_id == push.consts.target_id) {
            finalColor += light.color.rgb * light.intensity;
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
        if (light.type == 0) {
            vec3 spec = (D * F * G / (4.0 * dotNL * dotNV + 0.001));
            finalColor += shadow_term * over_dist_squared * lcol * dotNL * (kD * albedo + spec);
        }
        
        /* Rectangle light */
        else if (light.type == 1)
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

            finalColor += shadow_term * geometric_term * lcol * (spec + dcol*diff);
        }

        /* Disk Light */
        else if (light.type == 2)
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

            finalColor += shadow_term * geometric_term * lcol * (spec + dcol*diff);
        }
        
        /* Rod light */
        else if (light.type == 3) {
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

            finalColor += shadow_term * geometric_term * lcol * (spec + dcol*diff);
        }
        
        /* Sphere light */
        else if (light.type == 4)
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

            finalColor += shadow_term * geometric_term * lcol * (spec + dcol*diff);
        }
    }

    return finalColor;
}

// Final Color Contribution Function          ----------------------------------------------------
vec4 get_color_contribution(inout MaterialStruct material, vec3 N, vec3 V, vec3 P, vec3 MP, vec2 UV, vec4 VC)
{
    /* Compute reflection and refraction directions */
    vec3 R = -normalize(reflect(V, N));	
	float eta = 1.0 / material.ior; // air = 1.0 / material = ior
	vec3 Refr = normalize(refract(-V, N, eta));

    float metallic = material.metallic;
	float transmission = material.transmission;
	float roughness = getRoughness(material, UV, MP);
	float transmission_roughness = getTransmissionRoughness(material);
	vec4 albedo = getAlbedo(material, VC, UV, MP);
	float alphaMask = getAlphaMask(material, UV, MP);
    float alpha = (alphaMask < 1.0) ? alphaMask : albedo.a; // todo: move alpha out of albedo. 
    
    /* TODO: Add support for ray tracing */
	#ifndef RAYTRACING
    if (alpha == 0.0) discard;
    #endif

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
	vec3 ambient = (kD * diffuse + specular_reflection + specular_refraction) * ao;

    // Iterate over point lights
	vec3 finalColor = ambient + get_light_contribution(P, V, N, albedo_mix, albedo.rgb, metallic, roughness);

    // Tone mapping
	finalColor = Uncharted2Tonemap(finalColor * push.consts.exposure);
	finalColor = finalColor * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

    // Gamma correction
	finalColor = pow(finalColor, vec3(1.0f / push.consts.gamma));

    return vec4(finalColor, alpha);
}

#endif