#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MAXLIGHTS 3

layout(binding = 0) uniform CameraBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewinv;
    mat4 projinv;
    float near;
    float far;
} cbo;

layout(binding = 1) uniform UniformBufferObject {
  mat4 modelMatrix;
  mat4 normalMatrix;
  vec4 ka;
  vec4 kd;
  vec4 ks;
} ubo;

struct LightObject {
	vec4 position;
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	mat4 model;
	mat4 view;
	mat4 fproj;
	mat4 bproj;
	mat4 lproj;
	mat4 rproj;
	mat4 uproj;
	mat4 dproj;
};

layout(binding = 2) uniform LightBufferObject {
 LightObject lights[MAXLIGHTS];
 int numLights;
} lbo;

layout(binding = 3) uniform sampler2D reflectionSampler;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 position;
layout(location = 2) in vec4 clipPos;

layout(location = 0) out vec4 outColor;

void main() {
	/*Blinn-Phong shading model
		I = ka + Il * kd * (l dot n) + Il * ks * (h dot n)^N */	
    
    /* Use the eye space position as the texture coordinate
        into a reflected scene rendered to texture. */

	vec3 ndc = clipPos.xyz / clipPos.w; //perspective divide/normalize
    vec2 screenPos = ndc.xy * 0.5 + 0.5; //ndc is -1 to 1 in GL. scale for 0 to 1


	vec4 texColor = textureLod(reflectionSampler, screenPos, 50.0);

	vec3 ambientColor = vec3(0.0,0.0,0.0);
	vec3 diffuseColor = vec3(0.0,0.0,0.0);
	vec3 specularColor = vec3(0.0,0.0,0.0);

	/* All computations are in eye space */
	vec3 v = vec3(0.0, 0.0, 1.0);
	vec3 n = normalize(normal);
	for (int i = 0; i < lbo.numLights; ++i) {
		vec3 l = normalize(vec3(cbo.view * lbo.lights[i].position) - position);
		vec3 h = normalize(v + l);
		float diffterm = max(dot(l, n), 0.0);
		float specterm = max(dot(h, n), 0.0);

		ambientColor += texColor.rgb * ubo.ka.rgb * lbo.lights[i].ambient.rgb;

		diffuseColor += texColor.rgb * ubo.kd.rgb * lbo.lights[i].diffuse.rgb * diffterm;
		
		specularColor += texColor.rgb * ubo.ks.rgb * lbo.lights[i].specular.rgb * pow(specterm, 80.);
	}

    outColor = vec4(diffuseColor, ubo.kd.a * texColor.a) + vec4(specularColor, ubo.ks.a * texColor.a) + vec4(ambientColor, ubo.ka.a * texColor.a);
}