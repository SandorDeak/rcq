#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0) uniform gbuffer_gen_data
{
	mat4 proj;
	mat4 view;
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


void main()
{
	mat4 model_to_view=data.view*tr.model; 
	
	vec4 pos0=model_to_view*vec4(tr.scale*pos_in, 1.0f);
	gl_Position=data.proj*pos0;
	
	pos_out=pos0.xyz;
	tex_coord_out=tex_coord_in*tr.tex_scale;	
	TBN_out=mat3(model_to_view)*mat3(tangent_in, bitangent_in, normal_in);
}