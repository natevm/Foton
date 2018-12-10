#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 worldPositionFrag;
layout(location = 1) in vec3 normalFrag;
layout(location = 2) in vec2 texCoordFrag;

#define MAXLIGHTS 10
struct PointLightBufferItem {
  vec4 color;
  int falloffType;
  float intensity;
  int pad2;
  int pad3;

  /* For shadow mapping */
  mat4 localToWorld;
  mat4 worldToLocal;
  mat4 fproj;
  mat4 bproj;
  mat4 lproj;
  mat4 rproj;
  mat4 uproj;
  mat4 dproj;
};


layout(binding = 0) uniform PerspectiveBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewinv;
    mat4 projinv;
    float near;
    float far;
} pbo;

layout(binding = 1) uniform TransformBufferObject {
    mat4 worldToLocal;
    mat4 localToWorld;
} tbo;

layout(binding = 2) uniform PointLightBufferObject {
    PointLightBufferItem lights[MAXLIGHTS];
    int totalLights;
} plbo;

layout(binding = 3) uniform MaterialBufferObject {
  	vec4 ka, kd, ks;
	float diffuseReflectivity;
	float specularReflectivity;
	float emissivity;
	float transparency;
	bool useDiffuseTexture, useSpecularTexture, useShadowMap;
} mbo;

layout(binding = 4) uniform sampler2D diffuseSampler;
layout(binding = 5) uniform sampler2D specSampler;
layout(binding = 6, rgba32f) writeonly uniform image3D volume;
layout(binding = 7) uniform samplerCube shadowMap;

layout(location = 0) out vec4 outColor;

vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);   

float traceShadowMap(int lightIndex, vec3 w_from, vec3 w_light) {
  // get vector between fragment position and light position
  vec3 fragToLight = w_from - w_light;
  // now get current linear depth as the length between the fragment and light position
  float currentDepth = length(fragToLight);

  float farPlane = 1000.f; // todo: read from pbo

  float shadow = 0.0;
  float bias   = 0.05;
  int samples  = 20;
  float viewDistance = currentDepth;//length(viewPos - fragPos);
  float diskRadius = .01;//(1.0 + (viewDistance / farPlane)) / 100.0;  
  for(int i = 0; i < samples; ++i)
  {
      float closestDepth = texture(shadowMap, fragToLight.xzy).r;
      closestDepth *= farPlane;   // Undo mapping [0;1]
      shadow += 1.0f;
      if((currentDepth - closestDepth) > 0.15)
          shadow -= min(1.0, pow((currentDepth - closestDepth), .1));
  }
  shadow /= float(samples);  

  return shadow;
}


bool isInsideCube(const vec3 p, float e) { return abs(p.x) < 1 + e && abs(p.y) < 1 + e && abs(p.z) < 1 + e; }

void main(){
  vec3 pos = worldPositionFrag * .1;
    //imageStore(volume, ivec3(0, 0, 0), vec4(1.0, 0.0, 1.0, 1.0));

	if(!isInsideCube(pos, 0)) return;
	// Calculate diffuse lighting fragment contribution.


    /*Blinn-Phong shading model
		I = ka + Il * kd * (l dot n) + Il * ks * (h dot n)^N */	
	vec4 diffMatColor = (mbo.useDiffuseTexture) ? texture(diffuseSampler, texCoordFrag) : mbo.kd;
	vec4 specMatColor = (mbo.useSpecularTexture) ? texture(specSampler, texCoordFrag) : mbo.ks;

	vec3 ambientColor = vec3(0.0,0.0,0.0);
	vec3 diffuseColor = vec3(0.0,0.0,0.0);
	vec3 specularColor = vec3(0.0,0.0,0.0);

  ambientColor += vec3(mbo.ka);

  float alpha = pow(1 - mbo.transparency, 4);// * ((diffuseColor.x + diffuseColor.y + diffuseColor.z) * .333); // For soft shadows to work better with transparent materials.


	/* All computations are in eye space */
	vec3 v = vec3(0.0, 0.0, 1.0);
	vec3 n = normalize(normalFrag);
	for (int i = 0; i < plbo.totalLights; ++i) {
		vec3 lightPos = vec3(plbo.lights[i].localToWorld[3]);
		vec3 lightCol = vec3(plbo.lights[i].color) * plbo.lights[i].intensity;
		vec3 l = normalize(lightPos - worldPositionFrag);
		vec3 h = normalize(v + l);
		float diffterm = max(dot(l, n), 0.0);
		float specterm = max(dot(h, n), 0.0);
		float dist = 1.0;

		float shadowBlend = 1;
  	if(mbo.useShadowMap && diffterm > 0)
    		shadowBlend = traceShadowMap(i, worldPositionFrag, lightPos);

		if (plbo.lights[i].falloffType == 1) dist /= (1.0 + pow(distance(lightPos, worldPositionFrag), 2.0f));
		else if (plbo.lights[i].falloffType == 2) dist /= (1.0 + distance(lightPos, worldPositionFrag));

		diffuseColor += vec3(diffMatColor) * shadowBlend * lightCol * diffterm * dist;
		
		specularColor += vec3(specMatColor)  * shadowBlend * lightCol * pow(specterm, 80.) * dist;
	}

  //alpha *= (1.0 - (diffuseColor.r + diffuseColor.g + diffuseColor.b) * .333);

	// Output lighting to 3D texture.
	/* Go from -1,1 to 0,1 */
	vec3 voxel = 0.5f * pos + vec3(0.5f); 
	ivec3 dim = imageSize(volume);
	vec4 res = alpha * vec4(vec3(diffuseColor + specularColor + ambientColor), 1.0);
    imageStore(volume, ivec3(dim * voxel), res);
}