/* Inputs */
layout(location = 0) in vec3 w_normal;
layout(location = 1) in vec3 w_position;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 w_cameraPos;
layout(location = 4) in vec4 vert_color;
layout(location = 5) in vec3 m_position;
layout(location = 6) in vec3 s_position;

/* Outputs */
layout(location = 0) out vec4 outColor;

/* Definitions */
#define PI 3.1415926535897932384626433832795


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

// --- unfiltered checkerboard ---
float checkersTexture( in vec3 p )
{
    vec3 q = floor(p);
    return mod( q.x+q.y+q.z, 2.0 );            // xor pattern
}

// From GPU gems
float filterwidth(in vec3 v)
{
    vec3 fw = max(abs(dFdxFine(v)), abs(dFdyFine(v)));
    return max(max(fw.x, fw.y), fw.z);
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

vec4 getAlbedo()
{
	EntityStruct entity = ebo.entities[push.consts.target_id];
	MaterialStruct material = mbo.materials[entity.material_id];
	vec4 albedo = material.base_color; 

	/* If the use vertex colors flag is set, use the vertex color as the base color instead. */
	if ((material.flags & (1 << 0)) != 0)
		albedo = vert_color;

	if (material.base_color_texture_id != -1) {
        TextureStruct tex = txbo.textures[material.base_color_texture_id];

        /* Raster texture */
        if (tex.type == 0)
        {
  		    albedo = texture(sampler2D(texture_2Ds[material.base_color_texture_id], samplers[tex.sampler_id]), fragTexCoord);
        }

        else if (tex.type == 1)
        {
            float mate = checker(m_position, tex.scale);
            // If I do gamma correction here on linux, I get weird nans...
            return mix(tex.color1, tex.color2, mate);
        }
    }

	return vec4(pow(albedo.rgb, vec3(push.consts.gamma)), albedo.a);
}

vec4 sampleVolume(vec3 position, float lod)
{
    EntityStruct entity = ebo.entities[push.consts.target_id];
	MaterialStruct material = mbo.materials[entity.material_id];
    TextureStruct tex = txbo.textures[material.volume_texture_id];

    vec4 color = vec4(1.0, 0.0, 0.0, 0.00);

    if (tex.type == 0)
    {
        color = texture(sampler3D(texture_3Ds[material.volume_texture_id], samplers[tex.sampler_id]), position);
    }
    else if (tex.type == 1)
    {
        float mate = checker(position, tex.scale);//texture(sampler3D(texture_3Ds[material.volume_texture_id], samplers[material.volume_texture_id]), normalize(position) );
        color = mix(tex.color1, tex.color2, mate);
    }

    //textureLod(volumeTexture, pos, mbo.lod);
    return color;//vec4(color.rgb, 0.0);
}


float getRoughness(MaterialStruct material)
{
	float roughness = material.roughness;

	if (material.roughness_texture_id != -1) {
        TextureStruct tex = txbo.textures[material.roughness_texture_id];
        if (tex.sampler_id != -1) 
        {
  		    roughness = texture(sampler2D(texture_2Ds[material.roughness_texture_id], samplers[tex.sampler_id]), fragTexCoord).r;
        }
	}

	return roughness;
}

vec2 sampleBRDF(vec3 N, vec3 V, float roughness)
{
    TextureStruct tex = txbo.textures[push.consts.brdf_lut_id];

	roughness = clamp(roughness, 0.0, 1.0);
	return texture( 
			sampler2D(texture_2Ds[push.consts.brdf_lut_id], samplers[tex.sampler_id]), 
			vec2(max(dot(N, V), 0.0), roughness)
	).rg;
}

// Todo: add support for sampled irradiance
vec3 sampleIrradiance(vec3 N)
{
	vec3 adjusted = vec3(N.x, N.z, N.y);

	if (push.consts.diffuse_environment_id != -1) 
    {
        TextureStruct tex = txbo.textures[push.consts.diffuse_environment_id];
        if (tex.sampler_id != -1) {
            return texture(
                samplerCube(texture_cubes[push.consts.diffuse_environment_id], samplers[tex.sampler_id]), adjusted
            ).rgb;
        }
	}
	
	return getSky(adjusted);
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


// Todo: add support for prefiltered reflection 
vec3 prefilteredReflection(vec3 R, float roughness)
{
	vec3 reflection = vec3(0.0, 0.0, 0.0);
	
	roughness = clamp(roughness, 0.0, 1.0);
	vec3 adjusted = vec3(R.x, R.z, R.y);

	if (push.consts.specular_environment_id != -1) {
        TextureStruct tex = txbo.textures[push.consts.specular_environment_id];
		
		float lod = roughness * max(tex.mip_levels, 0.0);
		float lodf = floor(lod);
		float lodc = ceil(lod);
		
		vec3 a = textureLod(
			samplerCube(texture_cubes[push.consts.specular_environment_id], samplers[tex.sampler_id]), 
			adjusted, lodf).rgb;

		vec3 b = textureLod(
			samplerCube(texture_cubes[push.consts.specular_environment_id], samplers[tex.sampler_id]), 
			adjusted, lodc).rgb;

		reflection = mix(a, b, lod - lodf);
	}
	 else {
		reflection = mix(getSky(adjusted), (push.consts.top_sky_color.rgb + push.consts.bottom_sky_color.rgb) * .5, roughness);
	}

	return reflection;
}

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

	return getSky(dir);// + getSun(dir);


	// vec3 up = vec3(0.0, 1.0, 0.0);
	// float alpha = dot(up, dir);
	// return vec3(alpha, alpha, alpha);
}
