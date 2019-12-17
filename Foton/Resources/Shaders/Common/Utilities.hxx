#ifndef UTILITIES_HXX
#define UTILITIES_HXX

#include "Foton/Resources/Shaders/Common/Random.hxx"

/* MATH UTILITIES */
float saturate(float v)
{
    return clamp(v, 0.0, 1.0);
}

float pow2(float x) { 
    return x*x;
}

float luminance(const in vec3 c) {
	return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

float average_vec(const in vec3 c) {
    return (c.x + c.y + c.z) * .3333333;
}

void createBasis(vec3 normal, out vec3 tangent, out vec3 binormal){
    if (abs(normal.x) > abs(normal.y)) {
        tangent = normalize(vec3(0., normal.z, -normal.y));
    }
    else {
        tangent = normalize(vec3(-normal.z, 0., normal.x));
    }    
    binormal = cross(normal, tangent);
}

void directionOfAnisotropicity(vec3 normal, out vec3 tangent, out vec3 binormal){
    tangent = cross(normal, vec3(1.,0.,1.));
    binormal = normalize(cross(normal, tangent));
    tangent = normalize(cross(normal,binormal));
}

float distanceSq(vec3 v1, vec3 v2) {
    vec3 d = v1 - v2;
    return dot(d, d);
}

bool sameHemiSphere(const in vec3 wo, const in vec3 wi, const in vec3 normal) {
    return dot(wo, normal) * dot(wi, normal) > 0.0;
}

vec3 sphericalDirection(float sinTheta, float cosTheta, float sinPhi, float cosPhi) {
    return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

vec3 uniformSampleCone(vec2 u12, float cosThetaMax, vec3 xbasis, vec3 ybasis, vec3 zbasis) {
    float cosTheta = (1. - u12.x) + u12.x * cosThetaMax;
    float sinTheta = sqrt(1. - cosTheta * cosTheta);
    float phi = u12.y * TWO_PI;
    vec3 samplev = sphericalDirection(sinTheta, cosTheta, sin(phi), cos(phi));
    return samplev.x * xbasis + samplev.y * ybasis + samplev.z * zbasis;
}

vec2 concentricSampleDisk(const in vec2 u) {
    vec2 uOffset = 2. * u - vec2(1., 1.);

    if (uOffset.x == 0. && uOffset.y == 0.) return vec2(0., 0.);

    float theta, r;
    if (abs(uOffset.x) > abs(uOffset.y)) {
        r = uOffset.x;
        theta = PI/4. * (uOffset.y / uOffset.x);
    } else {
        r = uOffset.y;
        theta = PI/2. - PI/4. * (uOffset.x / uOffset.y);
    }
    return r * vec2(cos(theta), sin(theta));
}

vec3 cosineSampleHemisphere(const in vec2 u) {
    vec2 d = concentricSampleDisk(u);
    float z = sqrt(max(EPSILON, 1. - d.x * d.x - d.y * d.y));
    return vec3(d.x, d.y, z);
}

vec3 uniformSampleHemisphere(const in vec2 u) {
    float z = u[0];
    float r = sqrt(max(EPSILON, 1. - z * z));
    float phi = 2. * PI * u[1];
    return vec3(r * cos(phi), r * sin(phi), z);
}

float powerHeuristic(float nf, float fPdf, float ng, float gPdf){
    float f = nf * fPdf;
    float g = ng * gPdf;
    return (f*f)/(f*f + g*g);
}

vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

/* TESTS */
bool
test_reprojected_normal(vec3 n1, vec3 n2)
{
	return dot(n1, n2) > 0.95;
}

bool
test_inside_screen(ivec2 p, ivec2 res)
{
	return all(greaterThanEqual(p, ivec2(0)))
		&& all(lessThan(p, res));
}

bool
test_reprojected_depth(float z1, float z2, float dz)
{
	float z_diff = abs(z1 - z2);
	return z_diff < 2.0 * (dz + 1e-3);
}


/* TEXTURE UTILITIES */
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
    // #ifndef RAYTRACING
    // vec3 fw = max(abs(dFdxFine(v)), abs(dFdyFine(v)));
    // return max(max(fw.x, fw.y), fw.z);
    // #else
    // #endif
    vec3 q = floor(v);
    return mod( q.x+q.y+q.z, 2.0 );            // xor pattern
}

// Uses IQ's filtering method recommended here: https://www.shadertoy.com/view/XsfGDn
vec2 get_filtered_uv(ivec2 textureResolution, vec2 uv) 
{
    // float textureResolution = 64.0;
	uv = uv*textureResolution + 0.5;
	vec2 iuv = floor( uv );
	vec2 fuv = fract( uv );
	uv = iuv + fuv*fuv*(3.0-2.0*fuv); // fuv*fuv*fuv*(fuv*(fuv*6.0-15.0)+10.0);
	uv = (uv - 0.5)/textureResolution;
    return uv;
}

float checker(in vec3 uvw, float scale)
{
    float width = filterwidth(uvw*scale);
    vec3 p0 = (uvw * scale) - .5 * width;
    vec3 p1 = (uvw * scale) + .5 * width;
    #define BUMPINT(x) (floor((x)/2.0f) + 2.f * max( ((x)/2.0f ) - floor((x)/2.0f ) - .5f, 0.f))
    vec3 i = (BUMPINT(p1) - BUMPINT(p0)) / width;
    return step(.5, i.x * i.y * i.z + (1.0f - i.x) * (1.0f - i.y) * (1.0f - i.z));
}

vec4 sample_texture_2D(vec4 default_color, int texture_id, vec2 uv, vec3 m_position)
{
    vec4 color = default_color; 
    if ((texture_id < 0) || (texture_id >= max_textures)) return default_color;

    TextureStruct tex = txbo.textures[texture_id];
    
    if ((tex.sampler_id < 0) || (tex.sampler_id >= max_samplers))  return default_color;

    /* Avoids linear filtering artifacts */
    if (tex.sampler_id == LINEAR_SAMPLER) {
        uv = get_filtered_uv(ivec2(tex.width, tex.height), uv);
    }

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
    if ((texture_id < 0) || (texture_id >= max_textures)) return default_color;
    vec4 color = default_color;

    TextureStruct tex = txbo.textures[texture_id];
    
    if ((tex.sampler_id < 0) || (tex.sampler_id >= max_samplers))  return default_color;

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
    return sample_texture_2D(vec4(material.base_color.a), material.alpha_texture_id, uv, m_position).r;
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
    #if !defined(CAMERA_RAYTRACING) && !defined(GLOBAL_RAYTRACING) && !defined(CAMERA_COMPUTE) && !defined(COMPUTE)
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

void get_origin_and_direction(in ivec2 pixel_seed, int frame_seed, in mat4 projinv, in mat4 viewinv, in ivec2 pixel_coords, in ivec2 frame_size, out vec3 origin, out vec3 direction) {
    vec2 pixel_center = vec2(pixel_coords.xy) + vec2(random(), random()) * .5;// * PATH_TRACE_TILE_SIZE;
    // if (is_progressive_refinement_enabled()) pixel_center += vec2(random(), random()) * .5 * PATH_TRACE_TILE_SIZE;
	const vec2 in_uv = pixel_center/vec2(frame_size);
	vec2 d = in_uv * 2.0 - 1.0; d.y *= -1.0;
    vec4 t = (projinv * vec4(d.x, d.y, 1, 1));
    vec3 target = t.xyz / t.w;
    origin = (viewinv * vec4(0,0,0,1)).xyz;
    direction = (viewinv * vec4(target, 0)).xyz ;
    direction = normalize(direction);
}

void unpack_entity(
    int entity_id,
    inout EntityStruct entity, inout TransformStruct transform,
    inout MaterialStruct material, inout LightStruct light
) {
    if ((entity_id < 0) || (entity_id >= max_entities)) return;
    entity = ebo.entities[entity_id];
    if (entity.initialized != 1) return;
    if ((entity.transform_id < 0) || (entity.transform_id >= max_transforms)) return;
    transform = tbo.transforms[entity.transform_id];
    if ((entity.material_id >= 0) && (entity.material_id < max_materials)) 
        material = mbo.materials[entity.material_id];
    if ((entity.light_id >= 0) && (entity.light_id < max_lights))
        light = lbo.lights[entity.light_id];
}



void unpack_entity_struct(
    int entity_id, inout EntityStruct entity
) {
    if ((entity_id < 0) || (entity_id >= max_entities)) return;
    entity = ebo.entities[entity_id];
}

void unpack_material_struct(
    int entity_id, const in EntityStruct entity, vec2 uv, vec3 m_position, inout MaterialStruct material
) {
    if ((entity_id < 0) || (entity_id >= max_entities)) return;
    if (entity.initialized != 1) return;
    material = mbo.materials[entity.material_id];

    material.base_color = getAlbedo(material, vec4(0.0), uv, m_position);
    float mask = getAlphaMask(material, uv, m_position);
    material.base_color.a = (mask < material.base_color.a) ? mask : material.base_color.a;
}

void unpack_material_struct(
    int material_id, vec2 uv, vec3 m_position, inout MaterialStruct material
) {
    if ((material_id < 0) || (material_id >= max_materials)) return;
    material = mbo.materials[material_id];

    material.base_color = getAlbedo(material, vec4(0.0), uv, m_position);
    float mask = getAlphaMask(material, uv, m_position);
    material.base_color.a = (mask < material.base_color.a) ? mask : material.base_color.a;
}

void unpack_light_struct(
    int entity_id, const in EntityStruct entity, inout LightStruct light
) {
    if ((entity_id < 0) || (entity_id >= max_entities)) return;
    if (entity.initialized != 1) return;
    if ((entity.light_id < 0) || (entity.light_id >= max_lights)) return;
    light = lbo.lights[entity.light_id];
}

void unpack_light_struct(
    int light_id, inout LightStruct light
) {
    if ((light_id < 0) || (light_id >= max_lights)) return;
    light = lbo.lights[light_id];
}

// bool validate() {
//     #ifdef RAYTRACING
// if (int(gl_LaunchIDNV.x * PATH_TRACE_TILE_SIZE) >= push.consts.width) return false;
//     if (int(gl_LaunchIDNV.y * PATH_TRACE_TILE_SIZE) >= push.consts.height) return false;    
//     #endif
//     if ((push.consts.ltc_mat_lut_id < 0) || (push.consts.ltc_mat_lut_id >= max_textures)) return false;
//     if ((push.consts.ltc_amp_lut_id < 0) || (push.consts.ltc_amp_lut_id >= max_textures)) return false;
//     return true;
// }


ivec2 get_reprojected_pixel(int frame, ivec2 ipos) 
{
    int grad_rand = frame % (GRADIENT_TILE_SIZE * GRADIENT_TILE_SIZE);
    ivec2 grad_offset = ivec2(grad_rand / GRADIENT_TILE_SIZE, grad_rand % GRADIENT_TILE_SIZE);
    ivec2 grad_tile = ivec2(ipos / GRADIENT_TILE_SIZE);
    ivec2 grad_pt_tile = GRADIENT_TILE_SIZE * grad_tile + grad_offset;
    return grad_pt_tile;
}

#endif