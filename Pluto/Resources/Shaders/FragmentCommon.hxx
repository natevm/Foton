layout(location = 0) in vec3 w_normal;
layout(location = 1) in vec3 w_position;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 w_cameraPos;
layout(location = 4) in vec4 vert_color;
layout(location = 5) in vec3 m_position;
layout(location = 6) in vec3 s_position;

layout(location = 0) out vec4 outColor;

// --- unfiltered checkerboard ---
float checkersTexture( in vec3 p )
{
    vec3 q = floor(p);
    return mod( q.x+q.y+q.z, 2.0 );            // xor pattern
}

// From GPU gems

float filterwidth(vec3 v)
{
  vec3 fw = max(abs(dFdx(v)), abs(dFdy(v)));
  return max(max(fw.x, fw.y), fw.z); // might not need z...
}

float checker(vec3 uvw, float scale)
{
  float width = filterwidth(uvw*scale);
  vec3 p0 = (uvw * scale) - .5 * width;
  vec3 p1 = (uvw * scale) + .5 * width;
  #define BUMPINT(x) (floor((x)/2) + 2.f * max( ((x)/2.0f ) - floor((x)/2.0f ) - .5f, 0.f))
  vec3 i = (BUMPINT(p1) - BUMPINT(p0)) / width;
  return i.x * i.y * i.z + (1 - i.x) * (1 - i.y) * (1 - i.z);
}

vec4 getAlbedo()
{
	EntityStruct entity = ebo.entities[push.consts.target_id];
	MaterialStruct material = mbo.materials[entity.material_id];
	vec4 albedo = material.base_color; 

	/* If the use vertex colors flag is set, use the vertex color as the base color instead. */
	if ((material.flags & (1 << 0)) != 0)
		albedo = vert_color;

	if (material.base_color_texture_id != -1) {
        TextureStruct tex = txbo.textures[material.base_color_texture_id];

        /* Raster texture */
        if (tex.type == 0)
        {
  		    albedo = texture(sampler2D(texture_2Ds[material.base_color_texture_id], samplers[material.base_color_texture_id]), fragTexCoord);
        }

        else if (tex.type == 1)
        {
            float mate = checker(m_position, tex.scale);
            albedo = mix(tex.color1, tex.color2, mate);
        }
    }

	return vec4(pow(albedo.rgb, vec3(2.2)), albedo.a);
}