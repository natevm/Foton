#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable

#include "Pluto/Resources/Shaders/DescriptorLayout.hxx"

layout(location = 0) in vec3 w_normal;
layout(location = 1) in vec3 w_position;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 w_reflection;

layout(location = 0) out vec4 outColor;

void main() {
  EntityStruct entity = ebo.entities[push.consts.target_id];
  MaterialStruct material = mbo.materials[entity.material_id];

  // EntityStruct camera_entity = ebo.entities[push.consts.camera_id];
  // CameraStruct camera = cbo.cameras[camera_entity.camera_id];
  // TransformStruct camera_transform = tbo.transforms[camera_entity.transform_id];

  vec4 base_color = (material.base_color_texture_id == -1) ? material.base_color : 
    texture(sampler2D(texture_2Ds[material.base_color_texture_id], samplers[material.base_color_texture_id]), fragTexCoord);
  
  /*Blinn-Phong shading model
    I = ka + Il * kd * (l dot n) + Il * ks * (h dot n)^N */	

  // //vec3 ambientColor = vec3(0.0,0.0,0.0);
  vec3 diffuseColor = vec3(0.0,0.0,0.0);
  // vec3 specularColor = vec3(0.0,0.0,0.0);
  // //vec3 reflectColor = vec3(texture(samplerCubeMap, inRefl));

  
  // vec3 v = vec3(0.0, 0.0, 1.0);
  vec3 n = normalize(w_normal);
  vec3 temp = vec3(0.0, 0.0, 3.0);
  /* Forward light pass */
  for (int i = 0; i < MAX_LIGHTS; ++i) {

    int light_entity_id = push.consts.light_entity_ids[i];
    if (light_entity_id == -1) continue;

    EntityStruct light_entity = ebo.entities[light_entity_id];

    if ( (light_entity.initialized != 1) || (light_entity.transform_id == -1)) continue;
    LightStruct light = lbo.lights[light_entity.light_id];

    /* Objects which are lights glow */
    if (light_entity_id == push.consts.target_id) {
      diffuseColor += light.diffuse.rgb;
    }
    else 
    {
      TransformStruct light_transform = tbo.transforms[light_entity.transform_id];
      vec3 l_p = vec3(light_transform.localToWorld[3]);
      vec3 l = normalize(l_p - w_position);
      //   vec3 h = normalize(v + l);
        float diffterm = max(dot(l, n), 0.0);
      //   float specterm = max(dot(h, n), 0.0);

      //   //ambientColor += vec3(ubo.ka) * vec3(lbo.lights[i].ambient);

        diffuseColor += base_color.rgb * vec3(light.diffuse) * diffterm;
        
      //   specularColor += vec3(lbo.lights[i].specular) * vec3(ubo.ks) * pow(specterm, 80.);
    }
  }

  outColor = vec4(diffuseColor.xyz, 1.0);//material.base_color;//vec4((diffuseColor + specularColor), 1.0);
}