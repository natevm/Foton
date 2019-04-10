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

/* Descriptor Sets */
layout(std430, set = 0, binding = 0) readonly buffer EntitySSBO    { EntityStruct entities[]; } ebo;
layout(std430, set = 0, binding = 1) readonly buffer TransformSSBO { TransformStruct transforms[]; } tbo;
layout(std430, set = 0, binding = 2) readonly buffer CameraSSBO    { CameraStruct cameras[]; } cbo;
layout(std430, set = 0, binding = 3) readonly buffer MaterialSSBO  { MaterialStruct materials[]; } mbo;
layout(std430, set = 0, binding = 4) readonly buffer LightSSBO     { LightStruct lights[]; } lbo;

layout(std430, set = 1, binding = 0) readonly buffer TextureSSBO           { TextureStruct textures[]; } txbo;
layout(set = 1, binding = 1) uniform sampler samplers[MAX_SAMPLERS];
layout(set = 1, binding = 2) uniform texture2D texture_2Ds[MAX_TEXTURES];
layout(set = 1, binding = 3) uniform textureCube texture_cubes[MAX_TEXTURES];
layout(set = 1, binding = 4) uniform texture3D texture_3Ds[MAX_TEXTURES];

/* Push Constants */
layout(std430, push_constant) uniform PushConstants {
    PushConsts consts;
} push;

bool is_multiview_enabled()
{
    return ((push.consts.flags & (1 << 0)) != 0);
}

bool is_reverse_z_enabled()
{
    return ((push.consts.flags & (1 << 1)) != 0);
}