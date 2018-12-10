#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable

#define GLSL
#include "Pluto/Entity/EntityStruct.hxx"
#include "Pluto/Material/MaterialStruct.hxx"
#include "Pluto/Light/LightStruct.hxx"
#include "Pluto/Transform/TransformStruct.hxx"
#include "Pluto/Camera/CameraStruct.hxx"

#define MAX_MULTIVIEW 6
layout(std430, binding = 0) readonly buffer EntitySSBO    { EntityStruct entities[]; } ebo;
layout(std430, binding = 1) readonly buffer TransformSSBO { TransformStruct transforms[]; } tbo;
layout(std430, binding = 2) readonly buffer CameraSSBO    { CameraStruct cameras[]; } cbo;
layout(std430, binding = 3) readonly buffer MaterialSSBO  { MaterialStruct materials[]; } mbo;
layout(std430, binding = 4) readonly buffer LightSSBO     { LightStruct lights[]; } lbo;

layout(location = 0) in vec3 point;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texcoord;

layout(push_constant) uniform PushConsts {
	int entity_id;
    int camera_id;
} pushConsts;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    EntityStruct entity = ebo.entities[pushConsts.entity_id];
    CameraStruct camera = cbo.cameras[pushConsts.camera_id];
    MaterialStruct material = mbo.materials[entity.material_id];
    TransformStruct transform = tbo.transforms[entity.transform_id];
    
    gl_Position =  camera.multiviews[gl_ViewIndex].proj * camera.multiviews[gl_ViewIndex].view * transform.localToWorld * vec4(point, 1.0);
    gl_PointSize = 1.0;
    fragColor = material.base_color;
    fragTexCoord = texcoord;
}