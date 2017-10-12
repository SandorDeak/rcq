#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0) uniform em_gen_mat
{
	mat4 view;
	vec3 dir;
	uint padding0;
	vec3 irradiance;
	uint padding1;
	vec3 ambient_irradiance;
	uint padding2;
	
} data;

layout(set=1, binding=0) uniform transform_data
{
	mat4 model;
	vec3 scale;
	uint padding0;
	vec2 tex_scale;
} tr;



layout(location=0) in vec3 pos_in;
layout(location=1) in vec3 normal_in;
layout(location=2) in vec2 tex_coord_in;

layout(location=0) out vec3 normal_out;
layout(location=1) out vec2 tex_coord_out;

void main()
{
	normal_out=mat3(tr.model)*normal_in;
	tex_coord_out=tr.tex_scale*tex_coord_in;
	gl_Position=data.view*tr.model*vec4(pos_in*tr.scale, 1.f);
}