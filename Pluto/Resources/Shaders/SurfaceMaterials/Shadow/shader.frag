#version 450
#extension GL_ARB_separate_shader_objects : enable
precision highp float;

layout(location = 0) in vec4 w_pos;
layout(location = 1) in vec4 w_lightPos;

layout(location = 0) out vec4 outColor;

layout(binding = 2) uniform MaterialBufferObject {
    vec4 color;
    float pointSize;
    bool useTexture;
} mbo;

layout(binding = 3) uniform sampler2D colorTex;

void main() {
  // get distance between fragment and light source 
  float dist = distance(w_pos.xyz, w_lightPos.xyz);

  // map to [0;1] range by dividing by far plane. 
  dist /= 1000.f;// todo: read from pbo.

  // write as modified depth
  outColor = vec4(dist, dist, dist, 1.0);
  gl_FragDepth = dist; 
}
