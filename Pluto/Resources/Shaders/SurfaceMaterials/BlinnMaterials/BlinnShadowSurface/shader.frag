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

struct LightObject { /* TODO: maybe change this name */
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

layout(binding = 3) uniform sampler2D LShadowMap;
layout(binding = 4) uniform sampler2D RShadowMap;
layout(binding = 5) uniform sampler2D UShadowMap;
layout(binding = 6) uniform sampler2D DShadowMap;
layout(binding = 7) uniform sampler2D FShadowMap;
layout(binding = 8) uniform sampler2D BShadowMap;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 position;
layout(location = 2) in vec3 w_position;

layout(location = 0) out vec4 outColor;

float textureProj(vec4 fP, vec4 bP, vec4 lP, vec4 rP, vec4 uP, vec4 dP, vec2 off)
{
	float shadow = 1.0;

	vec4 scf = fP / fP.w;
	scf.x = scf.x * .5 + .5;
	scf.y = scf.y * .5 + .5;

	vec4 scb = bP / bP.w;
	scb.x = scb.x * .5 + .5;
	scb.y = scb.y * .5 + .5;

	vec4 scl = lP / lP.w;
	scl.x = scl.x * .5 + .5;
	scl.y = scl.y * .5 + .5;

	vec4 scr = rP / rP.w;
	scr.x = scr.x * .5 + .5;
	scr.y = scr.y * .5 + .5;

	vec4 scu = uP / uP.w;
	scu.x = scu.x * .5 + .5;
	scu.y = scu.y * .5 + .5;

	vec4 scd = dP / dP.w;
	scd.x = scd.x * .5 + .5;
	scd.y = scd.y * .5 + .5;

	/*Forward */
	if ( scf.z > 0.0 && scf.z < 1.0 
		&& scf.x > 0.0 && scf.x < 1.0
		&& scf.y > 0.0 && scf.y < 1.0) 
	{
		float dist = texture( FShadowMap, scf.st + off ).r;
		if ( scf.w > 0.0 && dist < scf.z - .00005 ) 
		{
		 	shadow = 0.0;
		}
	} 

	/* Backward */
	else if ( scb.z > 0.0 && scb.z < 1.0 
		&& scb.x > 0.0 && scb.x < 1.0
		&& scb.y > 0.0 && scb.y < 1.0) 
	{
		float dist = texture( BShadowMap, scb.st + off ).r;
		if ( scb.w > 0.0 && dist < scb.z - .00005 ) 
		{
		 	shadow = 0.0;
		}
	}

	/* Left */
	else if ( scl.z > 0.0 && scl.z < 1.0 
		&& scl.x > 0.0 && scl.x < 1.0
		&& scl.y > 0.0 && scl.y < 1.0) 
	{
		float dist = texture( LShadowMap, scl.st + off ).r;
		if ( scl.w > 0.0 && dist < scl.z - .00005 ) 
		{
		 	shadow = 0.0;
		}
	}

	/* Right */
	else if ( scr.z > 0.0 && scr.z < 1.0 
		&& scr.x > 0.0 && scr.x < 1.0
		&& scr.y > 0.0 && scr.y < 1.0) 
	{
		float dist = texture( RShadowMap, scr.st + off ).r;
		if ( scr.w > 0.0 && dist < scr.z - .00005 ) 
		{
		 	shadow = 0.0;
		}
	}

	/* Up */
	else if ( scu.z > 0.0 && scu.z < 1.0 
		&& scu.x > 0.0 && scu.x < 1.0
		&& scu.y > 0.0 && scu.y < 1.0) 
	{
		float dist = texture( UShadowMap, scu.st + off ).r;
		if ( scu.w > 0.0 && dist < scu.z - .00005 ) 
		{
		 	shadow = 0.0;
		}
	}

	/* Down*/
	else if ( scd.z > 0.0 && scd.z < 1.0 
		&& scd.x > 0.0 && scd.x < 1.0
		&& scd.y > 0.0 && scd.y < 1.0) 
	{
		float dist = texture( DShadowMap, scd.st + off ).r;
		if ( scd.w > 0.0 && dist < scd.z - .00005 ) 
		{
		 	shadow = 0.0;
		}
	}


	return shadow;
}


void main() {
	/*Blinn-Phong shading model
		I = ka + Il * kd * (l dot n) + Il * ks * (h dot n)^N */	

	vec3 ambientColor = vec3(0.0,0.0,0.0);
	vec3 diffuseColor = vec3(0.0,0.0,0.0);
	vec3 specularColor = vec3(0.0,0.0,0.0);

	/* All computations are in eye space */
	vec3 v = vec3(0.0, 0.0, 1.0);
	vec3 n = normalize(normal);
	for (int i = 0; i < lbo.numLights; ++i) {
		
		/* Light space position = ??? */
		vec4 lfp = lbo.lights[i].fproj * lbo.lights[i].view * vec4(w_position, 1.0);
		vec4 lbp = lbo.lights[i].bproj * lbo.lights[i].view * vec4(w_position, 1.0);
		vec4 llp = lbo.lights[i].lproj * lbo.lights[i].view * vec4(w_position, 1.0);
		vec4 lrp = lbo.lights[i].rproj * lbo.lights[i].view * vec4(w_position, 1.0);
		vec4 lup = lbo.lights[i].uproj * lbo.lights[i].view * vec4(w_position, 1.0);
		vec4 ldp = lbo.lights[i].dproj * lbo.lights[i].view * vec4(w_position, 1.0);

		float shadow = textureProj(lfp, lbp, llp, lrp, lup, ldp, vec2(0.0));

		/* Computations below are in eye space */
		vec3 l = normalize(vec3(cbo.view * lbo.lights[i].position) - position);
		vec3 h = normalize(v + l);
		float diffterm = max(dot(l, n), 0.0);
		float specterm = max(dot(h, n), 0.0);
		float dist = 1.0 / (1.0 + pow(distance(vec3(cbo.view * lbo.lights[i].position), position), 2.0f));

		ambientColor += vec3(ubo.ka) * vec3(lbo.lights[i].ambient);

		diffuseColor += vec3(ubo.kd) * vec3(lbo.lights[i].diffuse) * diffterm * dist * shadow;
		
		specularColor += vec3(lbo.lights[i].specular) * vec3(ubo.ks) * pow(specterm, 80.) * dist * shadow;
	}

    outColor = vec4(diffuseColor + specularColor + ambientColor, 1.0);
}