#version 450
#extension GL_ARB_separate_shader_objects : enable


const uint FRUSTUM_SPLIT_COUNT=2;

layout(set=0, binding=0) uniform ss_dir_shadow_map_gen_data
{
	mat4 projs[FRUSTUM_SPLIT_COUNT];
	float near;
	float far;
} data;

layout(set=0, binding=1) uniform sampler2DArray csm_tex;

layout(input_attachment_index=0, set=0, binding=2) uniform subpassInput pos_in;

layout(location=0) out float shadow_out;

float calc_index(float z)
{
	float val=(z-data.near)/(data.far-data.near);
	return sqrt(val)*FRUSTUM_SPLIT_COUNT;
}

void main()
{
	vec4 view_pos=subpassLoad(pos_in);
	float split_index=calc_index(-view_pos.z);
	vec3 light_pos=(data.projs[uint(split_index)]*view_pos).xyz;
	float shadow=1.f;	
	if(light_pos.z>texture(csm_tex, vec3(0.5*light_pos.xy+vec2(0.5), split_index)).x)
	{
		shadow=0.f;
	}
	shadow_out=shadow;
}