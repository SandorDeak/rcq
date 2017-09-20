#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0) uniform camera_data
{
	mat4 proj_x_view;
	vec3 pos;
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
layout(location=2) in vec2 texcoords_in;

layout(location=0) out vec3 color_out;

void main()
{
	gl_Position=cam.proj_x_view*tr.model*vec4(pos_in, 1.f);
	color_out=normal_in;
}