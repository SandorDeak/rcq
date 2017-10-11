#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(set=1, binding=0) uniform transform_data
{
	mat4 model;
	vec3 scale;
	uint padding0;
	vec2 tex_scale;
} tr;

layout(location=0) in pos_in;

void main()
{
	gl_Position=tr.model*vec4(pos_in, 1.f);
}
