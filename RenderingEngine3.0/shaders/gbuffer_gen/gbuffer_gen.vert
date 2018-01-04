#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0) uniform gbuffer_gen_data
{
	mat4 proj_x_view;
	vec3 cam_pos;
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
layout(location=3) in vec3 tangent_in;
layout(location=4) in vec3 bitangent_in;

layout(location=0) out vec2 tex_coord_out;
layout(location=1) out mat3 TBN_out;
layout(location=4) out vec3 pos_out;
layout(location=5) out vec3 view_out;


void main()
{	
	vec4 pos_world=tr.model*vec4(tr.scale*pos_in, 1.0f);
	gl_Position=data.proj_x_view*pos_world;	
	pos_out=pos_world.xyz;
	tex_coord_out=tex_coord_in*tr.tex_scale;
	view_out=data.cam_pos-pos_world.xyz;
	TBN_out=mat3(tr.model)*mat3(tangent_in, bitangent_in, normal_in); //from tangent to world
}