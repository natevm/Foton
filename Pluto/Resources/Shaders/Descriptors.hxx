#define GLSL

/* Extensions */
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable
#extension GL_EXT_nonuniform_qualifier : enable

/* Component Declarations */
#include "Pluto/Material/PushConstants.hxx"
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

layout(std430, set = 1, binding = 0) readonly buffer TextureSSBO           { TextureStruct textures[]; } txbo;
layout(set = 1, binding = 1) uniform sampler samplers[MAX_SAMPLERS];
layout(set = 1, binding = 2) uniform texture2D texture_2Ds[MAX_TEXTURES];
layout(set = 1, binding = 3) uniform textureCube texture_cubes[MAX_TEXTURES];
layout(set = 1, binding = 4) uniform texture3D texture_3Ds[MAX_TEXTURES];

layout(set = 2, binding = 0, std430) readonly buffer PositionBuffer {
    vec4 positions[];
} PositionBuffers[];

layout(set = 3, binding = 0, std430) readonly buffer NormalBuffer {
    vec4 normals[];
} NormalBuffers[];

layout(set = 4, binding = 0, std430) readonly buffer ColorBuffer {
    vec4 colors[];
} ColorBuffers[];

layout(set = 5, binding = 0, std430) readonly buffer TexCoordBuffer {
    vec2 texcoords[];
} TexCoordBuffers[];

layout(set = 6, binding = 0, std430) readonly buffer IndexBuffer {
    int indices[];
} IndexBuffers[];

#ifdef RAYTRACING
layout(set = 7, binding = 0) uniform accelerationStructureNV topLevelAS;
layout(set = 7, binding = 1, rgba16f) uniform image2D render_image;
layout(set = 7, binding = 2, rgba16f) uniform image2D gbuffers[7];
#endif

/* Push Constants */
layout(std430, push_constant) uniform PushConstants {
    PushConsts consts;
} push;

bool is_multiview_enabled()
{
    return ((push.consts.flags & (1 << 0)) == 0);
}

bool is_reverse_z_enabled()
{
    return ((push.consts.flags & (1 << 1)) == 0);
}

struct HitInfo {
    vec4 N;
    vec4 P;
    vec4 C;
    vec2 UV;
    vec4 color;
    float distance;
    int entity_id; 
    int bounce_count;
    bool is_shadow_ray;
};
