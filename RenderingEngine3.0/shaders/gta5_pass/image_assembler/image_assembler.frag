#version 450
#extension GL_ARB_separate_shader_objects : enable

#define PI 3.1415926535897f


layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput F0_ssao_in;
layout(input_attachment_index=1, set=0, binding=1) uniform subpassInput albedo_ssds_in;


layout(set=0, binding=2) uniform sampler2D pos_roughness_in;
layout(set=0, binding=3) uniform sampler2D normal_ao_in;


layout(set=0, binding=4) uniform sampler2DArray em_tex;

layout(set=0, binding=5) uniform image_assembler_data
{
	vec3 dir; //should be in view space!!!
	uint padding0;
	vec3 irradiance;
	uint padding1;
	vec3 ambient_irradiance;
	uint padding2;
} data;

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
	vec4 pos_roughness=texture(pos_roughness_in, gl_FragCoord.xy);
	vec3 v=-normalize(pos_roughness.xyz);
	float roughness=pos_roughness.w;
	
	vec4 normal_ao=texture(normal_ao_in, gl_FragCoord.xy);
	vec3 n=normal_ao.xyz;
	float ao=normal_ao.w;
	
	vec4 F0_ssao_in=subpassLoad(F0_ssao_in);
	vec3 F0=F0_ssao_in.xyz;
	float ssao=F0_ssao_in.w;
	
	vec4 albedo_ssds=subpassLoad(albedo_ssds_in);
	vec3 albedo=albedo_ssds.xyz;
	float ssds=albedo_ssds.w;
	
	ao*=ssao;
	
	vec3 l=-data.dir;
	vec3 h=normalize(l+v);
	
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
	
	vec3 color=(specular+lambert)*ssds*data.irradiance*l_dot_n+albedo*ao*data.ambient_irradiance;
	//vec3 color=(specular+lambert)*data.irradiance*l_dot_n+albedo*data.ambient_irradiance;
	
	color_out=vec4(color, 1.f);	
}