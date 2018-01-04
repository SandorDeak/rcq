#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint FRUSTUM_SPLIT_COUNT=2;

layout (set=0, binding=0) uniform csm_data
{
	mat4 view;
	mat4 projs[FRUSTUM_SPLIT_COUNT];
	float near;
	float far;
} csm;

layout(set=0, binding=1) uniform sampler2DArray cascade_shadow_maps;

layout(input_attachment_index=0, set=1, binding=0) uniform subpassInput pos_in;

layout(location=0) out vec4 shadow_out;

void main()
{
	vec4 pos_world=vec4(subpassLoad(pos_in).xyz, 1.f);
	vec4 pos_view=csm.view*pos_world;
	float index=pow(abs(pos_view.z-csm.near)/(csm.far-csm.near), 0.5f)*FRUSTUM_SPLIT_COUNT;
	vec3 pos_light=(csm.projs[uint(index)]*pos_view).xyz;
	shadow_out.w=pos_light.z>texture(cascade_shadow_maps, vec3(pos_light.xy, index)).x ? 1.f:0.f;	
}

