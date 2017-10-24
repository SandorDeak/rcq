#version 450
#extension GL_ARB_separate_shader_objects : enable


const uint FRUSTUM_SPLIT_COUNT=4;

layout(set=0, binding=0) uniform ss_dir_shadow_map_gen_data
{
	mat4 projs[FRUSTUM_SPLIT_COUNT];
	vec3 light_dir;
	float near;
	float far;
} data;

layout(set=0, binding=1) uniform sampler2DArray csm_tex;

layout(input_attachment_index=0, set=0, binding=2) uniform subpassInput pos_in;
layout(input_attachment_index=1, set=0, binding=3) uniform subpassInput normal_in;

float calc_index(float z)
{
	float val=(z-data.near)/(data.far-data.near);
	return pow(val, 0.25f)*float(FRUSTUM_SPLIT_COUNT);
}

void main()
{
	vec4 view_pos=vec4(subpassLoad(pos_in).xyz, 1.f);
	float bias=max(0.00001f, (1.f+dot(data.light_dir, subpassLoad(normal_in).xyz))*0.001f);
	float split_index=floor(calc_index(-view_pos.z));
	vec3 light_pos=(data.projs[uint(split_index)]*view_pos).xyz;
	float shadow=1.f;	
	if(light_pos.z>bias+texture(csm_tex, vec3(0.5*light_pos.xy+vec2(0.5), split_index)).x)
	{
		shadow=0.f;
	}
	
	gl_FragDepth=shadow;
}