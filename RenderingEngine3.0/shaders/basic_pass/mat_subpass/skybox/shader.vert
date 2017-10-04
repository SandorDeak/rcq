#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0) uniform camera_data
{
	mat4 proj_x_view;
	vec3 pos;
	uint padding0;
} cam;

const vec3 vertices[8]=vec3[8](
	vec3(-1.f, -1.f, -1.f),
	vec3(-1.f, -1.f, 1.f),
	vec3(-1.f, 1.f, -1.f),
	vec3(-1.f, 1.f, 1.f),
	vec3(1.f, -1.f, -1.f),
	vec3(1.f, -1.f, 1.f),
	vec3(1.f, 1.f, -1.f),
	vec3(1.f, 1.f, 1.f)
	);
	
layout(location=0) out vec3 tex_coord;


void main()
{
	tex_coord=vertices[gl_VertexIndex];
	vec4 pos=cam.proj_x_view*vec4(vertices[gl_VertexIndex]+cam.pos, 1.0);
	gl_Position=pos.xyww;
}
