#define GLSL
#include "Foton/Resources/Shaders/Common/ShaderConstants.hxx"
#include "Foton/Resources/Shaders/Common/GBufferLocations.hxx"

/* Precision */
precision highp float;

/* Extensions */
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_gpu_shader_int64 : enable

/* Component Declarations */
#include "Foton/Systems/RenderSystem/PushConstants.hxx"
#include "Foton/Entity/EntityStruct.hxx"
#include "Foton/Material/MaterialStruct.hxx"
#include "Foton/Light/LightStruct.hxx"
#include "Foton/Transform/TransformStruct.hxx"
#include "Foton/Camera/CameraStruct.hxx"
#include "Foton/Texture/TextureStruct.hxx"
#include "Foton/Mesh/MeshStruct.hxx"

/* Specialization constants */
layout (constant_id = 0) const int max_entities = MAX_ENTITIES;
layout (constant_id = 1) const int max_materials = MAX_MATERIALS;
layout (constant_id = 2) const int max_lights = MAX_LIGHTS;
layout (constant_id = 3) const int max_transforms = MAX_TRANSFORMS;
layout (constant_id = 4) const int max_cameras = MAX_CAMERAS;
layout (constant_id = 5) const int max_textures = MAX_TEXTURES;
layout (constant_id = 6) const int max_samplers = MAX_SAMPLERS;
layout (constant_id = 7) const int max_meshes = MAX_MESHES;

/* Descriptor Sets */
layout(std430, set = 0, binding = 0) readonly buffer EntitySSBO     { EntityStruct entities[]; } ebo;
layout(std430, set = 0, binding = 1) readonly buffer TransformSSBO  { TransformStruct transforms[]; } tbo;
layout(std430, set = 0, binding = 2) readonly buffer CameraSSBO     { CameraStruct cameras[]; } cbo;
layout(std430, set = 0, binding = 3) readonly buffer MaterialSSBO   { MaterialStruct materials[]; } mbo;
layout(std430, set = 0, binding = 4) readonly buffer LightSSBO      { LightStruct lights[]; } lbo;
layout(std430, set = 0, binding = 5) readonly buffer MeshSSBO       { MeshStruct meshes[]; } mesh_ssbo;
layout(std430, set = 0, binding = 6) readonly buffer LightIDBuffer  { int lightIDs[]; } lidbo;

layout(std430, set = 1, binding = 0) readonly buffer TextureSSBO    { TextureStruct textures[]; } txbo;
layout(set = 1, binding = 1) uniform sampler samplers[];
layout(set = 1, binding = 2) uniform texture2D texture_2Ds[];
layout(set = 1, binding = 3) uniform textureCube texture_cubes[];
layout(set = 1, binding = 4) uniform texture3D texture_3Ds[];
layout(set = 1, binding = 5) uniform texture3D BlueNoiseTile;

/* Push Constants */
layout(std430, push_constant) uniform PushConstants {
    PushConsts consts;
} push;

/* If used for primary visibility, rasterizer will write to these g buffers via corresponding framebuffer attachments. */
#if defined RASTER && defined PRIMARY_VISIBILITY
layout(location = ENTITY_MATERIAL_TRANSFORM_LIGHT_ADDR) out vec4 entity_material_transform_light;
layout(location = POSITION_DEPTH_ADDR) out vec4 position_depth;
layout(location = NORMAL_ADDR) out vec4 normal;
layout(location = DIFFUSE_SEED_ADDR) out vec4 seed_pixel;
layout(location = DIFFUSE_COLOR_ADDR) out vec4 albedo;
layout(location = DIFFUSE_MOTION_ADDR) out vec4 motion;
layout(location = UV_METALLIC_ROUGHESS_ADDR) out vec4 uv;
#endif

/* If used for shadows, only a depth buffer is needed. 
    Since we're using a compute shader to blur (VSM), we need a "color" framebuffer,
    since depth buffers cannot be read or written to. */
#if defined RASTER && defined SHADOW_MAP
layout(location = POSITION_DEPTH_ADDR) out vec4 outMoments;
#endif

/* Raytracer/compute can also write to the above, but via gbuffers directly */
#if defined  RAYTRACING || defined COMPUTE
layout(set = 2, binding = 0, rgba32f) uniform image2D render_image;
layout(set = 2, binding = 1, rgba32f) uniform image2D gbuffers[60];
layout(set = 2, binding = 2) uniform texture2D gbuffer_textures[60];
#endif

/* NV Ray tracing needs access to vert data and acceleration structure  */
#ifdef RAYTRACING
layout(set = 3, binding = 0, std430) readonly buffer PositionBuffer {
    vec4 positions[];
} PositionBuffers[];

layout(set = 4, binding = 0, std430) readonly buffer NormalBuffer {
    vec4 normals[];
} NormalBuffers[];

layout(set = 5, binding = 0, std430) readonly buffer ColorBuffer {
    vec4 colors[];
} ColorBuffers[];

layout(set = 6, binding = 0, std430) readonly buffer TexCoordBuffer {
    vec2 texcoords[];
} TexCoordBuffers[];

layout(set = 7, binding = 0, std430) readonly buffer IndexBuffer {
    int indices[];
} IndexBuffers[];

layout(set = 8, binding = 0) uniform accelerationStructureNV topLevelAS;
#endif

struct SurfaceInteraction
{
    int entity_id;
    vec4 m_p; 
    vec3 m_n; 
    vec3 m_t; 
    vec3 m_b; 
    vec3 w_i; 
    vec4 w_p; 
    vec3 w_n; 
    vec3 w_t; 
    vec3 w_b; 
    vec2 uv; 
    ivec2 pixel_coords;
    int random_dimension;
    int bounce_count;
    bool is_specular;
};

struct Contribution
{
    vec3 diffuse_radiance;
    vec3 specular_radiance;
    float alpha;
    int random_dimension;
};

struct HitInfo {
    vec3 m_p;
    vec3 w_p;
    vec3 m_n;
    vec3 w_n;
    vec2 uv;
    float distance;
    int entity_id; 
    bool is_shadow_ray;
    bool backface;
    float curvature;
    int tri;
    int mesh;
    vec2 barycentrics;
};

#if defined  RAYTRACING || defined COMPUTE

// TODO: remove this function
void unpack_gbuffer_data(in ivec2 tile, in ivec2 offset, out ivec2 ipos, out bool seed_found, out vec3 w_position, out float depth, out vec3 w_normal, 
    out int entity_id, out ivec2 pixel_seed, out int frame_seed, out vec3 albedo, out vec2 uv)
{
    vec4 temp;
    ipos = tile * PATH_TRACE_TILE_SIZE + offset;

    // Seed / Luminance G Buffer
    temp = imageLoad(gbuffers[DIFFUSE_SEED_ADDR], ipos);
    pixel_seed = ivec2(temp.xy);
    frame_seed = int(temp.z);

    // Position G Buffer 
    temp = imageLoad(gbuffers[POSITION_DEPTH_ADDR], ipos);
    w_position = temp.xyz;
    depth = temp.w;

    // Normal G Buffer
    temp = imageLoad(gbuffers[NORMAL_ADDR], ipos);
    w_normal = normalize(temp.xyz);

    // ID G Buffer
    temp = imageLoad(gbuffers[ENTITY_MATERIAL_TRANSFORM_LIGHT_ADDR], ipos);
    entity_id = int(temp.r);

    // UV G Buffer
    temp = imageLoad(gbuffers[UV_METALLIC_ROUGHESS_ADDR], ipos);
    uv = temp.xy;

    // Raw albedo
    temp = imageLoad(gbuffers[DIFFUSE_COLOR_ADDR], ipos);
    albedo = temp.xyz;
}
#endif