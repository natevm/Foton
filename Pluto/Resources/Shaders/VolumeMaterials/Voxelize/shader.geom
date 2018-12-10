#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 worldPositionGeom[];
layout(location = 1) in vec3 normalGeom[];
layout(location = 2) in vec2 texCoordGeom[];

layout(location = 0) out vec3 worldPositionFrag;
layout(location = 1) out vec3 normalFrag;
layout(location = 2) out vec2 texCoordFrag;

void main() {
	const vec3 p1 = worldPositionGeom[1] - worldPositionGeom[0];
	const vec3 p2 = worldPositionGeom[2] - worldPositionGeom[0];
	const vec3 p = abs(cross(p1, p2)); 
	for(uint i = 0; i < 3; ++i){
		worldPositionFrag = worldPositionGeom[i];
		normalFrag = normalGeom[i];
		texCoordFrag = texCoordGeom[i];
		if(p.z > p.x && p.z > p.y){
			gl_Position = vec4(worldPositionFrag.x*.1, worldPositionFrag.y*.1, 0, 1);
		} else if (p.x > p.y && p.x > p.z){
			gl_Position = vec4(worldPositionFrag.y*.1, worldPositionFrag.z*.1, 0, 1);
		} else {
			gl_Position = vec4(worldPositionFrag.x*.1, worldPositionFrag.z*.1, 0, 1);
		}
		EmitVertex();
	}
    EndPrimitive();
}