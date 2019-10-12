#define GLSL

/* Extensions */
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_gpu_shader_int64 : enable

/* Component Declarations */
#include "Pluto/Systems/RenderSystem/PushConstants.hxx"
#include "Pluto/Entity/EntityStruct.hxx"
#include "Pluto/Material/MaterialStruct.hxx"
#include "Pluto/Light/LightStruct.hxx"
#include "Pluto/Transform/TransformStruct.hxx"
#include "Pluto/Camera/CameraStruct.hxx"
#include "Pluto/Texture/TextureStruct.hxx"
#include "Pluto/Mesh/MeshStruct.hxx"

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

/* G buffer locations */
#define POSITION_DEPTH_ADDR 0
#define NORMAL_ID_ADDR 1
#define SEED_LUMINANCE_ADDR 2
#define ALBEDO_ADDR 3
#define MOTION_ADDR 4
#define UV_METALLIC_ROUGHESS_ADDR 5
#define POSITION_DEPTH_ADDR_PREV 6
#define NORMAL_ID_ADDR_PREV 7
#define SEED_LUMINANCE_ADDR_PREV 8
#define ALBEDO_ADDR_PREV 9
#define UV_METALLIC_ROUGHESS_ADDR_PREV 10
#define DIFFUSE_ILLUM_VAR_ADDR 11
#define SPECULAR_ILLUM_VAR_ADDR 12
#define PROGRESSIVE_HISTORY_ADDR 13
#define ATROUS_HISTORY_1_ADDR 14
#define ATROUS_HISTORY_2_ADDR 15
#define ATROUS_HISTORY_3_ADDR 22
#define SVGF_TAA_HISTORY_1_ADDR 16
#define SVGF_TAA_HISTORY_2_ADDR 17
#define SVGF_TAA_HISTORY_3_ADDR 18
#define SVGF_TAA_HISTORY_4_ADDR 19
#define SVGF_TAA_HISTORY_5_ADDR 24
#define SVGF_TAA_HISTORY_6_ADDR 25
#define SVGF_TAA_HISTORY_7_ADDR 32
#define SVGF_TAA_HISTORY_8_ADDR 33
#define TAA_HISTORY_1_ADDR 20
#define TAA_HISTORY_2_ADDR 21
#define TEMPORAL_GRADIENT_ADDR 26
#define TEMPORAL_GRADIENT_ADDR_PREV 27
#define LUMINANCE_VARIANCE_ADDR 28
#define LUMINANCE_VARIANCE_ADDR_PREV 23
#define DEBUG_ADDR 29
#define SAMPLE_COUNT_ADDR 30
#define SAMPLE_COUNT_ADDR_PREV 31
#define VARIANCE_ADDR 34

#define DEBUG_ADDR_NOISE 35
#define DEMODULATION_ADDR 36
// #define RAY_DIRECTION_ADDR_PREV 23 // free g buffer

/* If used for primary visibility, rasterizer will write to these g buffers via corresponding framebuffer attachments. */
#if defined RASTER && defined PRIMARY_VISIBILITY
layout(location = POSITION_DEPTH_ADDR) out vec4 position_depth;
layout(location = NORMAL_ID_ADDR) out vec4 normal_id;
layout(location = SEED_LUMINANCE_ADDR) out vec4 seed_luminance;
layout(location = ALBEDO_ADDR) out vec4 albedo;
layout(location = MOTION_ADDR) out vec4 motion;
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
layout(set = 2, binding = 1, rgba32f) uniform image2D gbuffers[40];
layout(set = 2, binding = 2) uniform texture2D gbuffer_textures[40];
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
};
