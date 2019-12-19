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

/* All shader types use texture and component descriptor sets */
#define COMPONENT_SET 0
#define TEXTURE_SET 1

/* Both compute and raster descriptor sets use the GBuffer descriptor set */
/* Ray tracing to camera gbuffers requires vertex descriptor sets to follow the gbuffer descriptor set */
/* Global ray tracing shaders not specific to a camera don't have the gbuffer descriptor set  */
#ifndef GLOBAL_RAYTRACING
    #define GBUFFER_SET 2
    #define POSITION_SET 3
    #define NORMAL_SET 4
    #define COLOR_SET 5
    #define TEXCOORD_SET 6
    #define INDEX_SET 7
    #define AS_SET 8
#else 
    #define POSITION_SET 2
    #define NORMAL_SET 3
    #define COLOR_SET 4
    #define TEXCOORD_SET 5
    #define INDEX_SET 6
    #define AS_SET 7
#endif

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
layout(std430, set = COMPONENT_SET, binding = 0) readonly buffer EntitySSBO     { EntityStruct entities[]; } ebo;
layout(std430, set = COMPONENT_SET, binding = 1) readonly buffer TransformSSBO  { TransformStruct transforms[]; } tbo;
layout(std430, set = COMPONENT_SET, binding = 2) readonly buffer CameraSSBO     { CameraStruct cameras[]; } cbo;
layout(std430, set = COMPONENT_SET, binding = 3) readonly buffer MaterialSSBO   { MaterialStruct materials[]; } mbo;
layout(std430, set = COMPONENT_SET, binding = 4) readonly buffer LightSSBO      { LightStruct lights[]; } lbo;
layout(std430, set = COMPONENT_SET, binding = 5) readonly buffer MeshSSBO       { MeshStruct meshes[]; } mesh_ssbo;
layout(std430, set = COMPONENT_SET, binding = 6) readonly buffer LightIDBuffer  { int lightIDs[]; } lidbo;

layout(std430, set = TEXTURE_SET, binding = 0) readonly buffer TextureSSBO    { TextureStruct textures[]; } txbo;
layout(set = TEXTURE_SET, binding = 1) uniform sampler samplers[];
layout(set = TEXTURE_SET, binding = 2) uniform texture2D texture_2Ds[];
layout(set = TEXTURE_SET, binding = 3) uniform textureCube texture_cubes[];
layout(set = TEXTURE_SET, binding = 4) uniform texture3D texture_3Ds[];
layout(set = TEXTURE_SET, binding = 5) uniform texture3D BlueNoiseTile;
layout(set = TEXTURE_SET, binding = 6) uniform texture2D BRDF_LUT;
layout(set = TEXTURE_SET, binding = 7) uniform texture2D LTC_MAT;
layout(set = TEXTURE_SET, binding = 8) uniform texture2D LTC_AMP;
layout(set = TEXTURE_SET, binding = 9) uniform textureCube Environment;
layout(set = TEXTURE_SET, binding = 10) uniform textureCube SpecularEnvironment;
layout(set = TEXTURE_SET, binding = 11) uniform textureCube DiffuseEnvironment;
layout(set = TEXTURE_SET, binding = 12) uniform texture2D DDGI_IRRADIANCE_TEXTURE;
layout(set = TEXTURE_SET, binding = 13) uniform texture2D DDGI_VISIBILITY_TEXTURE;
layout(set = TEXTURE_SET, binding = 14, rgba32f) uniform image2D DDGI_IRRADIANCE;
layout(set = TEXTURE_SET, binding = 15, rgba32f) uniform image2D DDGI_VISIBILITY;
layout(set = TEXTURE_SET, binding = 16, rgba32f) uniform image2D DDGI_GBUFFERS[5];

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
#if defined CAMERA_RAYTRACING || defined CAMERA_COMPUTE
layout(set = GBUFFER_SET, binding = 0, rgba32f) uniform image2D render_image;
layout(set = GBUFFER_SET, binding = 1, rgba32f) uniform image2D gbuffers[60];
layout(set = GBUFFER_SET, binding = 2) uniform texture2D gbuffer_textures[60];
#endif

/* NV Ray tracing needs access to vert data and acceleration structure  */
#if defined CAMERA_RAYTRACING || defined GLOBAL_RAYTRACING
layout(set = POSITION_SET, binding = 0, std430) readonly buffer PositionBuffer {
    vec4 positions[];
} PositionBuffers[];

layout(set = NORMAL_SET, binding = 0, std430) readonly buffer NormalBuffer {
    vec4 normals[];
} NormalBuffers[];

layout(set = COLOR_SET, binding = 0, std430) readonly buffer ColorBuffer {
    vec4 colors[];
} ColorBuffers[];

layout(set = TEXCOORD_SET, binding = 0, std430) readonly buffer TexCoordBuffer {
    vec2 texcoords[];
} TexCoordBuffers[];

layout(set = INDEX_SET, binding = 0, std430) readonly buffer IndexBuffer {
    int indices[];
} IndexBuffers[];

layout(set = AS_SET, binding = 0) uniform accelerationStructureNV topLevelAS;
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
