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

layout(set=1, binding=0) uniform sampler2D diffuse_tex;

layout(location=0) in vec3 normal_in;
layout(location=1) in vec2 tex_coord_in;

layout(location=0) out vec4 color_out;

void main()
{
	vec3 n=normalize(normal_in);
	float n_dot_l=max(dot(n, -data.dir), 0.f);
	vec3 color=texture(diffuse_tex, tex_coord_in).xyz*(data.ambient_irradiance+n_dot_l*data.irradiance);
	color_out=vec4(color, 1.f);
}