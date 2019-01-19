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