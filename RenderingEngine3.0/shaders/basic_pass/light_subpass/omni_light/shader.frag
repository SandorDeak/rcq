#version 450
#extension GL_ARB_separate_shader_objects : enable

#define PI 3.1415926535897f

layout(set=0, binding=0) uniform omni_light_data
{
	vec3 pos;
	uint padding0;
	vec3 radiance;
	
} old;

layout(set=0, binding=1) uniform samplerCube shadow_map;

//G-buffer
layout(input_attachment_index=0, set=1, binding=0) uniform subpassInput pos_in;
layout(input_attachment_index=1, set=1, binding=1) uniform subpassInput F0_roughness_in;
layout(input_attachment_index=2, set=1, binding=2) uniform subpassInput albedo_ao_in;
layout(input_attachment_index=3, set=1, binding=3) uniform subpassInput normal_in;
layout(input_attachment_index=4, set=1, binding=4) uniform subpassInput view_dir_in;

//output
layout(location=0) out vec4 color_out;

vec3 fresnel_schlick(float cos_theta, vec3 F0)
{
	return F0+(vec3(1.0f)-F0)*pow(1.0f-cos_theta, 5.0f);
}

float NDF(float n_dot_h_squared, float roughness)
{
	const float roughness_squared=pow(roughness, 2.0f);
	return roughness_squared/(PI*pow(n_dot_h_squared*(roughness_squared-1)+1, 2.0f));
}

float shlick_geometry_term(float dot_prod, float roughness)
{
	const float k=pow(roughness+1.0f, 2.0f)*0.125f;
	return dot_prod/(dot_prod*(1.0f-k)+k);
}

void main()
{	
	vec3 l=normalize(old.pos-subpassLoad(pos_in).xyz);
	vec3 v=subpassLoad(view_dir_in).xyz;
	float roughness=subpassLoad(F0_roughness_in).w;
	vec3 F0=subpassLoad(F0_roughness_in).xyz;
	vec3 albedo=subpassLoad(albedo_ao_in).xyz;
	float ao_factor=subpassLoad(albedo_ao_in).w;
	vec3 n=subpassLoad(normal_in).xyz;
	vec3 h=normalize(v+l);
	float l_dot_n=max(dot(l, n), 0.0f);
	float h_dot_n=max(dot(h,n), 0.0f);
	float h_dot_l=max(dot(h,l), 0.0f);
	float v_dot_n=max(dot(v,n), 0.0f);
	
	//PBR terms	
	vec3 fresnel=fresnel_schlick(h_dot_l, F0);
	float ndf=NDF(pow(h_dot_n, 2.0f), roughness);
	float geometry=shlick_geometry_term(l_dot_n, roughness)*shlick_geometry_term(v_dot_n, roughness);
	vec3 specular=fresnel*geometry*ndf;(4.0f*l_dot_n*v_dot_n+0.001f);		
	vec3 lambert=(1.0f-fresnel)*albedo/PI;
	//
	
	vec3 frag_color=(lambert+specular)*ao_factor*old.radiance*l_dot_n;
	
	//gamma correction
	//frag_color=pow(frag_color, vec3(1.f/2.2f));
	
	//set color_out
	color_out=vec4(frag_color, 1.f);
	
}
