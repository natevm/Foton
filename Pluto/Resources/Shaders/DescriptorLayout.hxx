#define GLSL
#include "Pluto/Material/PushConstants.hxx"
#include "Pluto/Entity/EntityStruct.hxx"
#include "Pluto/Material/MaterialStruct.hxx"
#include "Pluto/Light/LightStruct.hxx"
#include "Pluto/Transform/TransformStruct.hxx"
#include "Pluto/Camera/CameraStruct.hxx"
#include "Pluto/Texture/TextureStruct.hxx"

layout(std430, set = 0, binding = 0) readonly buffer EntitySSBO    { EntityStruct entities[]; } ebo;
layout(std430, set = 0, binding = 1) readonly buffer TransformSSBO { TransformStruct transforms[]; } tbo;
layout(std430, set = 0, binding = 2) readonly buffer CameraSSBO    { CameraStruct cameras[]; } cbo;
layout(std430, set = 0, binding = 3) readonly buffer MaterialSSBO  { MaterialStruct materials[]; } mbo;
layout(std430, set = 0, binding = 4) readonly buffer LightSSBO     { LightStruct lights[]; } lbo;

layout(set = 1, binding = 0) uniform sampler samplers[MAX_TEXTURES];
layout(set = 1, binding = 1) uniform texture2D texture_2Ds[MAX_TEXTURES];
layout(set = 1, binding = 2) uniform textureCube texture_cubes[MAX_TEXTURES];
layout(set = 1, binding = 3) uniform texture3D texture_3Ds[MAX_TEXTURES];

layout(push_constant) uniform PushConstants {
    PushConsts consts;
} push;