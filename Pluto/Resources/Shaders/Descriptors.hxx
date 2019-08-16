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

/* Descriptor Sets */
layout(std430, set = 0, binding = 0) readonly buffer EntitySSBO    { EntityStruct entities[]; } ebo;
layout(std430, set = 0, binding = 1) readonly buffer TransformSSBO { TransformStruct transforms[]; } tbo;
layout(std430, set = 0, binding = 2) readonly buffer CameraSSBO    { CameraStruct cameras[]; } cbo;
layout(std430, set = 0, binding = 3) readonly buffer MaterialSSBO  { MaterialStruct materials[]; } mbo;
layout(std430, set = 0, binding = 4) readonly buffer LightSSBO     { LightStruct lights[]; } lbo;
layout(std430, set = 0, binding = 5) readonly buffer MeshSSBO      { MeshStruct meshes[]; } mesh_ssbo;
layout(std430, set = 0, binding = 6) readonly buffer LightIDBuffer  { int lightIDs[]; } lidbo;

layout(std430, set = 1, binding = 0) readonly buffer TextureSSBO           { TextureStruct textures[]; } txbo;
layout(set = 1, binding = 1) uniform sampler samplers[MAX_SAMPLERS];
layout(set = 1, binding = 2) uniform texture2D texture_2Ds[MAX_TEXTURES];
layout(set = 1, binding = 3) uniform textureCube texture_cubes[MAX_TEXTURES];
layout(set = 1, binding = 4) uniform texture3D texture_3Ds[MAX_TEXTURES];

/* Rasterizer will only write to these, via framebuffer attachments. */
#if defined  RAYTRACING || defined COMPUTE
layout(set = 2, binding = 0, rgba16f) uniform image2D render_image;
/* GBUFFER 0 - Velocity in screen space */
/* GBUFFER 1 - History */
layout(set = 2, binding = 1, rgba16f) uniform image2D gbuffers[7];
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

/* Push Constants */
layout(std430, push_constant) uniform PushConstants {
    PushConsts consts;
} push;

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
    // vec4 C;
    vec2 uv;
    float distance;
    int entity_id; 
    bool is_shadow_ray;
    bool backface;

    // vec4 WN;
    // vec4 WP;
    // Contribution contribution;
    // vec2 pixel;
    // int bounce_count;
    // bool is_specular_ray;
    // int random_dimension;
};
