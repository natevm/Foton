/* Inputs */
layout(location = 0) in vec3 w_normal;
layout(location = 1) in vec3 w_position;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 w_cameraPos;
layout(location = 4) in vec4 vert_color;
layout(location = 5) in vec3 m_position;

/* Outputs */
layout(location = 0) out vec4 outColor;

/* Definitions */
#define PI 3.1415926535897932384626433832795
#define TWOPI 6.283185307

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

vec4 getAlbedo(inout MaterialStruct material)
{
	vec4 albedo = material.base_color; 

	/* If the use vertex colors flag is set, use the vertex color as the base color instead. */
	if ((material.flags & (1 << 0)) != 0)
		albedo = vert_color;

	if ((material.base_color_texture_id < 0) || (material.base_color_texture_id >= MAX_TEXTURES))
        return vec4(pow(albedo.rgb, vec3(push.consts.gamma)), albedo.a);
    
    TextureStruct tex = txbo.textures[material.base_color_texture_id];
    
    if ((tex.sampler_id < 0) || (tex.sampler_id >= MAX_SAMPLERS)) 
        return vec4(pow(albedo.rgb, vec3(push.consts.gamma)), albedo.a);

    /* Raster texture */
    if (tex.type == 0) {
        albedo = texture(sampler2D(texture_2Ds[material.base_color_texture_id], samplers[tex.sampler_id]), fragTexCoord);
    }

    /* Procedural checker texture */
    else if (tex.type == 1)
    {
        float mate = checker(m_position, tex.scale);
        // If I do gamma correction here on linux, I get weird nans...
        return mix(tex.color1, tex.color2, mate);
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


float getRoughness(inout MaterialStruct material)
{
	float roughness = material.roughness;

    if ((material.roughness_texture_id < 0) || (material.roughness_texture_id >= MAX_TEXTURES)) 
        return clamp(roughness, 0.0, 1.0);
    
    TextureStruct tex = txbo.textures[material.roughness_texture_id];
    
    if ((tex.sampler_id < 0) || (tex.sampler_id >= MAX_SAMPLERS)) 
        return clamp(roughness, 0.0, 1.0);

    return clamp(texture(sampler2D(texture_2Ds[material.roughness_texture_id], samplers[tex.sampler_id]), fragTexCoord).r, 0.0, 1.0);
}

float getTransmissionRoughness(inout MaterialStruct material)
{
    return clamp(material.transmission_roughness, 0.0, 1.0);
}

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

    return getSky(dir);


	// vec3 up = vec3(0.0, 1.0, 0.0);
	// float alpha = dot(up, dir);
	// return vec3(alpha, alpha, alpha);
}




/* LTC LIGHTING */
const float LUT_SIZE  = 64.0;
const float LUT_SCALE = (LUT_SIZE - 1.0)/LUT_SIZE;
const float LUT_BIAS  = 0.5/LUT_SIZE;


// Tracing and intersection
///////////////////////////

struct Ray
{
    vec3 origin;
    vec3 dir;
};

struct AABB {
	vec3 minimum;
	vec3 maximum;
};

struct Rect
{
    vec3  center;
    vec3  dirx;
    vec3  diry;
    float halfx;
    float halfy;

    vec4  plane;
};

// Camera functions
///////////////////

vec3 SolveCubic(vec4 Coefficient)
{
    // Normalize the polynomial
    Coefficient.xyz /= Coefficient.w;
    // Divide middle coefficients w_light_forward three
    Coefficient.yz /= 3.0;

    float A = Coefficient.w;
    float B = Coefficient.z;
    float C = Coefficient.y;
    float D = Coefficient.x;

    // Compute the Hessian and the discriminant
    vec3 Delta = vec3(
        -Coefficient.z*Coefficient.z + Coefficient.y,
        -Coefficient.y*Coefficient.z + Coefficient.x,
        dot(vec2(Coefficient.z, -Coefficient.y), Coefficient.xy)
    );

    float Discriminant = max(dot(vec2(4.0*Delta.x, -Delta.y), Delta.zy), 0.0);

    vec2 xlc, xsc;

    // Algorithm A
    {

        float A_a = 1.0; // n
        float C_a = Delta.x; // n
        float D_a = -2.0*B*Delta.x + Delta.y; // n

        // Take the cubic root of a normalized complex number
        float Theta = atan(sqrt(Discriminant), -D_a)/3.0; // y

        // return vec3(isnan(sqrt(max(Discriminant, 0.0))));

        float x_1a = 2.0*sqrt(-C_a)*cos(Theta);
        float x_3a = 2.0*sqrt(-C_a)*cos(Theta + (2.0/3.0)*PI);

        float xl;
        if ((x_1a + x_3a) > 2.0*B)
            xl = x_1a;
        else
            xl = x_3a;

        xlc = vec2(xl - B, A);
    }



    // Algorithm D
    {
        float A_d = D;
        float C_d = Delta.z;
        float D_d = -D*Delta.y + 2.0*C*Delta.z;

        // Take the cubic root of a normalized complex number
        float Theta = atan(D*sqrt(Discriminant), -D_d)/3.0;

        float x_1d = 2.0*sqrt(-C_d)*cos(Theta);
        float x_3d = 2.0*sqrt(-C_d)*cos(Theta + (2.0/3.0)*PI);

        float xs;
        if (x_1d + x_3d < 2.0*C)
            xs = x_1d;
        else
            xs = x_3d;

        xsc = vec2(-D, xs + C);
    }

    float E =  xlc.y*xsc.y;
    float F = -xlc.x*xsc.y - xlc.y*xsc.x;
    float G =  xlc.x*xsc.x;

    vec2 xmc = vec2(C*F - B*G, -B*F + C*E);

    vec3 Root = vec3(xsc.x/xsc.y, xmc.x/xmc.y, xlc.x/xlc.y);

    if (Root.x < Root.y && Root.x < Root.z)
        Root.xyz = Root.yxz;
    else if (Root.z < Root.x && Root.z < Root.y)
        Root.xyz = Root.xzy;

    return Root;
}

vec3 mul(mat3 m, vec3 v)
{
    return m * v;
}

mat3 mul(mat3 m1, mat3 m2)
{
    return m1 * m2;
}

float sqr(float x) { return x*x; }

vec3 rotation_y(vec3 v, float a)
{
    vec3 r;
    r.x =  v.x*cos(a) + v.z*sin(a);
    r.y =  v.y;
    r.z = -v.x*sin(a) + v.z*cos(a);
    return r;
}

vec3 rotation_z(vec3 v, float a)
{
    vec3 r;
    r.x =  v.x*cos(a) - v.y*sin(a);
    r.y =  v.x*sin(a) + v.y*cos(a);
    r.z =  v.z;
    return r;
}

vec3 rotation_yz(vec3 v, float ay, float az)
{
    return rotation_z(rotation_y(v, ay), az);
}

// code from [Frisvad2012]
void buildOrthonormalBasis(
in vec3 n, out vec3 b1, out vec3 b2)
{
    if (n.z < -0.9999999)
    {
        b1 = vec3( 0.0, -1.0, 0.0);
        b2 = vec3(-1.0,  0.0, 0.0);
        return;
    }
    float a = 1.0 / (1.0 + n.z);
    float b = -n.x*n.y*a;
    b1 = vec3(1.0 - n.x*n.x*a, b, -n.x);
    b2 = vec3(b, 1.0 - n.y*n.y*a, -n.y);
}

void generate_basis(vec3 v, out vec3 T1, out vec3 T2)
{
    vec3 basis1 = vec3(1.0, 0.0, 0.0);
    vec3 basis2 = vec3(0.0, 1.0, 1.0);

    float dot1 = dot(v, basis1);
    float dot2 = dot(v, basis2);

    if (dot1 < dot2)
    {
        T1 = normalize(basis1 - v*dot(basis1, v));
        T2 = normalize(cross(T1, v));
    }
    else {
        T1 = normalize(basis2 - v*dot(basis2, v));
        T2 = normalize(cross(T1, v));
    }
}

// mat3 transpose(mat3 v)
// {
//     mat3 tmp;
//     tmp[0] = vec3(v[0].x, v[1].x, v[2].x);
//     tmp[1] = vec3(v[0].y, v[1].y, v[2].y);
//     tmp[2] = vec3(v[0].z, v[1].z, v[2].z);

//     return tmp;
// }

mat3 mat3_from_columns(vec3 c0, vec3 c1, vec3 c2)
{
    mat3 m = mat3(c0, c1, c2);
    return m;
}

// Linearly Transformed Cosines
///////////////////////////////

vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5*inversesqrt(1.0 - x*x) - v;

    return cross(v1, v2) * theta_sintheta;
}

float IntegrateEdge(vec3 v1, vec3 v2)
{
    return IntegrateEdgeVec(v1, v2).z;
}

void ClipQuadToHorizon(inout vec3 L[5], out int n)
{
    // detect clipping config
    int config = 0;
    if (L[0].z > 0.0) config += 1;
    if (L[1].z > 0.0) config += 2;
    if (L[2].z > 0.0) config += 4;
    if (L[3].z > 0.0) config += 8;

    // clip
    n = 0;

    if (config == 0)
    {
        // clip all
    }
    else if (config == 1) // V1 clip V2 V3 V4
    {
        n = 3;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[3].z * L[0] + L[0].z * L[3];
    }
    else if (config == 2) // V2 clip V1 V3 V4
    {
        n = 3;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    }
    else if (config == 3) // V1 V2 clip V3 V4
    {
        n = 4;
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
        L[3] = -L[3].z * L[0] + L[0].z * L[3];
    }
    else if (config == 4) // V3 clip V1 V2 V4
    {
        n = 3;
        L[0] = -L[3].z * L[2] + L[2].z * L[3];
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
    }
    else if (config == 5) // V1 V3 clip V2 V4) impossible
    {
        n = 0;
    }
    else if (config == 6) // V2 V3 clip V1 V4
    {
        n = 4;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    }
    else if (config == 7) // V1 V2 V3 clip V4
    {
        n = 5;
        L[4] = -L[3].z * L[0] + L[0].z * L[3];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    }
    else if (config == 8) // V4 clip V1 V2 V3
    {
        n = 3;
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
        L[1] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] =  L[3];
    }
    else if (config == 9) // V1 V4 clip V2 V3
    {
        n = 4;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[2].z * L[3] + L[3].z * L[2];
    }
    else if (config == 10) // V2 V4 clip V1 V3) impossible
    {
        n = 0;
    }
    else if (config == 11) // V1 V2 V4 clip V3
    {
        n = 5;
        L[4] = L[3];
        L[3] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    }
    else if (config == 12) // V3 V4 clip V1 V2
    {
        n = 4;
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
    }
    else if (config == 13) // V1 V3 V4 clip V2
    {
        n = 5;
        L[4] = L[3];
        L[3] = L[2];
        L[2] = -L[1].z * L[2] + L[2].z * L[1];
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
    }
    else if (config == 14) // V2 V3 V4 clip V1
    {
        n = 5;
        L[4] = -L[0].z * L[3] + L[3].z * L[0];
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
    }
    else if (config == 15) // V1 V2 V3 V4
    {
        n = 4;
    }

    if (n == 3)
        L[3] = L[0];
    if (n == 4)
        L[4] = L[0];
}

float Fpo(float d, float l)
{
    return l/(d*(d*d + l*l)) + atan(l/d)/(d*d);
}

float Fwt(float d, float l)
{
    return l*l/(d*(d*d + l*l));
}

float I_diffuse_line(vec3 p1, vec3 p2)
{
    // tangent
    vec3 wt = normalize(p2 - p1);

    // clamping
    if (p1.z <= 0.0 && p2.z <= 0.0) return 0.0;
    if (p1.z < 0.0) p1 = (+p1*p2.z - p2*p1.z) / (+p2.z - p1.z);
    if (p2.z < 0.0) p2 = (-p1*p2.z + p2*p1.z) / (-p2.z + p1.z);

    // parameterization
    float l1 = dot(p1, wt);
    float l2 = dot(p2, wt);

    // shading point orthonormal projection on the line
    vec3 po = p1 - l1*wt;

    // distance to line
    float d = length(po);

    // integral
    float I = (Fpo(d, l2) - Fpo(d, l1)) * po.z +
              (Fwt(d, l2) - Fwt(d, l1)) * wt.z;
    return I / PI;
}

float I_ltc_line(mat3 m_inv, vec3 p1, vec3 p2)
{
    // transform to diffuse configuration
    vec3 p1o = m_inv * p1;
    vec3 p2o = m_inv * p2;
    float I_diffuse = I_diffuse_line(p1o, p2o);

    // width factor
    vec3 ortho = normalize(cross(p1, p2));
    float w =  1.0 / length(inverse(transpose(m_inv)) * ortho);

    return w * I_diffuse;
}

float D(mat3 m_inv, vec3 w)
{
    vec3 wo = m_inv * w;
    float lo = length(wo);
    float res = 1.0/PI * max(0.0, wo.z/lo) * abs(determinant(m_inv)) / (lo*lo*lo);
    return res;
}

float I_ltc_disks(mat3 m_inv, vec3 p1, vec3 p2, float R)
{
    float A = PI * R * R;
    vec3 wt  = normalize(p2 - p1);
    vec3 wp1 = normalize(p1);
    vec3 wp2 = normalize(p2);
    float Idisks = A * (
    D(m_inv, wp1) * max(0.0, dot(+wt, wp1)) / dot(p1, p1) +
    D(m_inv, wp2) * max(0.0, dot(-wt, wp2)) / dot(p2, p2));
    return Idisks;
}

vec3 LTC_Evaluate_Rect(
    vec3 N, vec3 V, vec3 P, mat3 m_inv, vec3 points[4], bool twoSided)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N*dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    m_inv = mul(m_inv, transpose(mat3(T1, T2, N)));

    // polygon (allocate 5 vertices for clipping)
    vec3 L[5];
    L[0] = mul(m_inv, points[0] - P);
    L[1] = mul(m_inv, points[1] - P);
    L[2] = mul(m_inv, points[2] - P);
    L[3] = mul(m_inv, points[3] - P);

    // integrate
    float sum = 0.0;

    vec3 dir = points[0].xyz - P;
    vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);

    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);
    
    vec3 vsum = vec3(0.0);
    
    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    float len = length(vsum);
    float z = vsum.z/len;
    
    if (behind)
        z = -z;
    
    vec2 uv = vec2(z*0.5 + 0.5, len);
    uv = uv*LUT_SCALE + LUT_BIAS;
    
    float scale = texture(sampler2D(texture_2Ds[push.consts.ltc_amp_lut_id], samplers[0]), uv).w;

    sum = len;//*scale;
    
    if (behind && !twoSided)
        sum = 0.0;

    if (isnan(sum)) return vec3(0.0, 0.0, 0.0);
    if (isinf(sum)) return vec3(0.0, 0.0, 0.0);
    
    vec3 Lo_i = vec3(sum, sum, sum);

    return Lo_i;
}


vec3 LTC_Evaluate_Rect_Clipped(
    vec3 N, vec3 V, vec3 P, mat3 m_inv, vec3 points[4], bool twoSided)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N*dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    m_inv = mul(m_inv, transpose(mat3(T1, T2, N)));

    // polygon (allocate 5 vertices for clipping)
    vec3 L[5];
    L[0] = mul(m_inv, points[0] - P);
    L[1] = mul(m_inv, points[1] - P);
    L[2] = mul(m_inv, points[2] - P);
    L[3] = mul(m_inv, points[3] - P);

    // integrate
    float sum = 0.0;

    int n;
    ClipQuadToHorizon(L, n);

    if (n == 0)
        return vec3(0, 0, 0);
    // project onto sphere
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);
    L[4] = normalize(L[4]);

    // integrate
    sum += IntegrateEdge(L[0], L[1]);
    sum += IntegrateEdge(L[1], L[2]);
    sum += IntegrateEdge(L[2], L[3]);
    if (n >= 4)
        sum += IntegrateEdge(L[3], L[4]);
    if (n == 5)
        sum += IntegrateEdge(L[4], L[0]);

    sum = twoSided ? abs(sum) : max(0.0, sum);

    if (isnan(sum)) return vec3(0.0, 0.0, 0.0);
    if (isinf(sum)) return vec3(0.0, 0.0, 0.0);
    
    vec3 Lo_i = vec3(sum, sum, sum);

    return Lo_i;
}

vec3 LTC_Evaluate_Disk(
    vec3 N, vec3 V, vec3 P, mat3 m_inv, vec3 points[4], bool twoSided)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N*dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    mat3 R = transpose(mat3(T1, T2, N));

    // polygon (allocate 5 vertices for clipping)
    vec3 L_[3];
    L_[0] = mul(R, points[0] - P);
    L_[1] = mul(R, points[1] - P);
    L_[2] = mul(R, points[2] - P);

    vec3 Lo_i = vec3(0);

    // init ellipse
    vec3 C  = 0.5 * (L_[0] + L_[2]);
    vec3 V1 = 0.5 * (L_[1] - L_[2]);
    vec3 V2 = 0.5 * (L_[1] - L_[0]);
    
    C  = m_inv * C;
    V1 = m_inv * V1;
    V2 = m_inv * V2;

    if(!twoSided && dot(cross(V1, V2), C) < 0.0)
        return vec3(0.0);

    // compute eigenvectors of ellipse
    float a, b;
    float d11 = dot(V1, V1);
    float d22 = dot(V2, V2);
    float d12 = dot(V1, V2);

    if (abs(d12)/sqrt(d11*d22) > 0.001)
    {
        float tr = d11 + d22;
        float det = -d12*d12 + d11*d22;

        // use sqrt matrix to solve for eigenvalues
        det = sqrt(max(det, 0.0));

        if ((tr - 2.0*det) < 0.0) return vec3(0.0, 0.0, 0.0);
        float u = 0.5*sqrt(tr - 2.0*det);
        float v = 0.5*sqrt(tr + 2.0*det);
        float e_max = sqr(u + v);
        float e_min = sqr(u - v);

        vec3 V1_, V2_;

        if (d11 > d22)
        {
            V1_ = d12*V1 + (e_max - d11)*V2;
            V2_ = d12*V1 + (e_min - d11)*V2;
        }
        else
        {
            V1_ = d12*V2 + (e_max - d22)*V1;
            V2_ = d12*V2 + (e_min - d22)*V1;
        }

        a = 1.0 / e_max;
        b = 1.0 / e_min;
        V1 = normalize(V1_);
        V2 = normalize(V2_);
    }
    else
    {
        a = 1.0 / dot(V1, V1);
        b = 1.0 / dot(V2, V2);
        V1 *= sqrt(a);
        V2 *= sqrt(b);
    }

    vec3 V3 = cross(V1, V2);
    if (dot(C, V3) < 0.0)
        V3 *= -1.0;

    // if (dot(normalize(V3), N) > .9)
        // return vec3(0.0, 0.0, 0.0);

    float L  = dot(V3, C);
    // float L  = dot(V3, C);
    if (L < .000001) return vec3(0.0, 0.0, 0.0);

    float x0 = clamp(dot(V1, C) / L, -1000000.0, 1000000.0);
    float y0 = clamp(dot(V2, C) / L, -1000000.0, 1000000.0);

    // if (any(isinf(vec2(x0,y0)))) return vec3(0.0, 0.0, 1.0);
    // if ((a < 0.0) || (b < 0.0)) return vec3(0.0, 0.0, 1.0);

    // if (any(isnan(vec3(L, x0, y0)))) return vec3(1.0, 0.0, 0.0);
    // return vec3(isinf(L), isinf(x0), isinf(y0));

    float E1 = inversesqrt(a);
    float E2 = inversesqrt(b);

    a *= L*L;
    b *= L*L;

    float c0 = a*b;
    float c1 = a*b*(1.0 + x0*x0 + y0*y0) - a - b;
    float c2 = 1.0 - a*(1.0 + x0*x0) - b*(1.0 + y0*y0);
    float c3 = 1.0;

    vec3 roots = SolveCubic(vec4(c0, c1, c2, c3));
    float e1 = roots.x;
    float e2 = roots.y;
    float e3 = roots.z;

    // When L goes to zero, c0 and c1 go to zero, c2 and c3 go to 1.
    // The solution to the cubic is -1
    // if (L < .001)    {
    //     e1 = e2 = e3 = -1;
    //     a = 0;
    //     b = 0;
    // }

    if (any(isnan(roots))) return vec3(0.0, 0.0, 0.0);

    vec3 avgDir = vec3(a*x0/(a - e2), b*y0/(b - e2), 1.0);

    mat3 rotate = mat3_from_columns(V1, V2, V3);

    avgDir = rotate*avgDir;
    avgDir = normalize(avgDir);

    float L1 = sqrt(-e2/e3);
    float L2 = sqrt(-e2/e1);

    float formFactor = L1*L2*inversesqrt((1.0 + L1*L1)*(1.0 + L2*L2));


    // use tabulated horizon-clipped sphere
    vec2 uv = vec2(avgDir.z*0.5 + 0.5, formFactor);
    uv = uv*LUT_SCALE + LUT_BIAS;
    float scale = texture(sampler2D(texture_2Ds[push.consts.ltc_amp_lut_id], samplers[0]), uv).w;

    float spec = formFactor*scale;

    if (isnan(spec)) return vec3(0.0, 0.0, 0.0);
    if (isinf(spec)) return vec3(0.0, 0.0, 0.0);

    Lo_i = vec3(spec, spec, spec);

    return vec3(Lo_i);
}

vec3 LTC_Evaluate_Rod(vec3 N, vec3 V, vec3 P, mat3 m_inv, vec3 points[2], float R, bool endCaps)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N*dot(V, N));
    T2 = cross(N, T1);

    mat3 B = transpose(mat3(T1, T2, N));

    vec3 p1 = mul(B, points[0] - P);
    vec3 p2 = mul(B, points[1] - P);

    float Iline = R * I_ltc_line(m_inv, p1, p2);
    float Idisks = endCaps ? I_ltc_disks(m_inv, p1, p2, R) : 0.0;

    if (isnan(Iline)) return vec3(0.0, 0.0, 0.0);
    if (isinf(Iline)) return vec3(0.0, 0.0, 0.0);
    if (isnan(Idisks)) return vec3(0.0, 0.0, 0.0);
    if (isinf(Idisks)) return vec3(0.0, 0.0, 0.0);

    return vec3(min(1.0, Iline + Idisks));
}

float saturate(float v)
{
    return clamp(v, 0.0, 1.0);
}

vec3 PowV3(vec3 v, float p)
{
    return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

const float gamma = 2.2;

vec3 ToLinear(vec3 v) { return PowV3(v,     gamma); }
vec3 ToSRGB(vec3 v)   { return PowV3(v, 1.0/gamma); }

float get_shadow_contribution(EntityStruct light_entity, LightStruct light, vec3 w_light_position, vec3 w_position) 
{
    float bias = .1;
    CameraStruct light_camera = cbo.cameras[light_entity.camera_id];
    int tex_id = light_camera.multiviews[0].tex_id;

    TransformStruct light_transform = tbo.transforms[light_entity.transform_id];
    vec3 l_position = (light_transform.worldToLocalRotation * light_transform.worldToLocalTranslation * vec4(w_position, 1.0)).xyz;
    vec3 pos_dir = -normalize(l_position);
    vec3 adjusted = vec3(pos_dir.x, -pos_dir.z, pos_dir.y);

    TextureStruct tex = txbo.textures[tex_id];
    float val = 1.0;
    if (tex.sampler_id != -1) {
        val = texture(
            samplerCube(texture_cubes[tex_id], samplers[tex.sampler_id]), adjusted
        ).r;

        val = (val + bias < length(l_position)) ? 0.0 : 1.0;
    }

    return val;
}

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
        
        /* Point light */
        if (light.type == 0) {
            if (dotNL < 0.0) continue;
            vec3 spec = (D * F * G / (4.0 * dotNL * dotNV + 0.001));
            float dist_squared = max(sqr(length(w_light_position - w_position)), 1.0);
            finalColor += shadow_term * (1.0 / dist_squared) * lcol * dotNL * (kD * albedo + spec);
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
