#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0) uniform camera_data
{
	mat4 proj_x_view;
	vec3 pos;
	uint padding0;
} cam;

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
layout(location=4) out vec3 pos_world_out;


void main()
{
	vec4 pos0=tr.model*vec4(tr.scale*pos_in, 1.0f);
	gl_Position=cam.proj_x_view*pos0;
	pos_world_out=pos0.xyz;
	tex_coord_out=tex_coord_in*tr.tex_scale;;
	vec3 n=(tr.model*vec4(normal_in, 0.0f)).xyz;
	vec3 t=(tr.model*vec4(tangent_in, 0.0f)).xyz;
	vec3 b=(tr.model*vec4(bitangent_in, 0.0f)).xyz;
	
	TBN_out=mat3(t, b, n); //tangent to world
}