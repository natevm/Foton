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

layout(location = 0) out vec3 w_normal;
layout(location = 1) out vec3 w_position;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 w_reflection;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    EntityStruct entity = ebo.entities[pushConsts.entity_id];
    CameraStruct camera = cbo.cameras[pushConsts.camera_id];
    MaterialStruct material = mbo.materials[entity.material_id];
    TransformStruct transform = tbo.transforms[entity.transform_id];

    w_position = vec3(transform.localToWorld * vec4(point.xyz, 1.0));
    vec3 w_cameraPos = vec3(camera.multiviews[gl_ViewIndex].viewinv[3]);
    vec3 w_viewDir = w_position - w_cameraPos;
    
    w_normal = normalize(vec3(transform.localToWorld * vec4(normal, 0)));
    vec3 reflection = reflect(w_viewDir, normalize(normal));
    
    /* From z up to y up */
    w_reflection.x = reflection.x;
    w_reflection.y = reflection.z;
    w_reflection.z = reflection.y;

    gl_Position = camera.multiviews[gl_ViewIndex].proj * camera.multiviews[gl_ViewIndex].view * vec4(w_position, 1.0);
    fragTexCoord = texcoord;
}