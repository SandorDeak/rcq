#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0) uniform omni_light_data
{
	vec3 pos;
	uint padding0;
	vec3 radiance;
	
} old;

layout(set=0, binding=1) uniform samplerCube shadow_map;

layout(set=1, binding=0) uniform transform_data
{
	mat4 model;
	vec3 scale;
	uint padding0;
	vec2 tex_scale;
} tr;


layout(location=0) in vec3 pos_in;

void main()
{
	gl_Position=(tr.model*vec4(tr.scale*pos_in, 1.f))-vec4(old.pos, 0.f);
}
