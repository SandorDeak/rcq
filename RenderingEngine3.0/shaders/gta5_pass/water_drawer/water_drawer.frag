#version 450
#extension GL_ARB_separate_shader_objects : enable

#define PI 3.1415926535897f

const vec3 water_albedo=vec3(35.f, 137.f, 218.f)/255.f;
const float water_roughness=0.2f;
const vec3 water_F0=vec3(0.02f);
const float index_of_refr=1.0003f/1.333f;

const float Rayleigh_scale_height=8000.f;
const float Mie_scale_height=1200.f;
const float Atmosphere_thickness=80000;
const float Earth_radius=6371000;
const vec3 Rayleigh_extinction_coefficient=vec3(5.8e-6, 1.35e-5, 3.31e-5)+vec3(3.426f, 8.298f, 0.356f)*0.06e-5;
const float Mie_extinction_coefficient=1.11f*2.e-6;


layout(set=0, binding=0) uniform water_drawer_data
{
	mat4 proj_x_view;
	mat4 mirrored_proj_x_view;
	vec3 view_pos;
	float height_bias;
	vec3 light_dir;
	vec3 irradiance;
	vec3 ambient_irradiance;
	vec2 tile_offset;
	vec2 tile_size_in_meter;
	vec2 half_resolution;
} data;


layout(set=0, binding=1) uniform sampler2D refraction_tex;
//layout(set=0, binding=2) uniform sampler2D reflection_tex;

layout(set=2, binding=0) uniform sampler3D Rayleigh_tex;
layout(set=2, binding=1) uniform sampler3D Mie_tex;
layout(set=2, binding=2) uniform sampler2D transmittance_tex;

layout(location=0) in vec3 normal_in;
layout(location=1) in vec3 pos_in;

layout(location=0) out vec4 color_out;

vec3 fresnel_schlick(float cos_theta, vec3 F0)
{
	return F0+(vec3(1.0f)-F0)*pow(1.0f-cos_theta, 5.0f);
}

float NDF(float n_dot_h_squared, float roughness)
{
	float roughness_squared=roughness*roughness;
	float part=n_dot_h_squared*(roughness_squared-1)+1.f;
	return roughness_squared/(PI*part*part);
}

float shlick_geometry_term(float dot_prod, float roughness)
{
	float k=pow(roughness+1.0f, 2.0f)*0.125f;
	return dot_prod/(dot_prod*(1.0f-k)+k);
}

float shlick_geometry_term_IBL(float dot_prod, float roughness)
{
	float k=pow(roughness, 2.0f)*0.5f;
	return dot_prod/(dot_prod*(1.0f-k)+k);
}

vec3 transmittance(vec3 p, vec3 q)
{
	float l=length(p-q);
	if (abs(p.y-q.y)<0.0000001f)
	{
		return l*(Rayleigh_extinction_coefficient*exp(-p.y/Rayleigh_scale_height)+vec3(Mie_extinction_coefficient)*exp(-p.y/Mie_scale_height));
	}
	
	vec3 Rayleigh=Rayleigh_extinction_coefficient*Rayleigh_scale_height*(exp(-q.y/Rayleigh_scale_height)-exp(-p.y/Rayleigh_scale_height));
	vec3 Mie=vec3(Mie_extinction_coefficient)*Mie_scale_height*(exp(-q.y/Mie_scale_height)-exp(-p.y/Mie_scale_height));
	return l*(Rayleigh+Mie)/(p.y-q.y);
}

vec3 params_to_tex_coords(vec3 params)
{
	vec3 tex_coords;
	tex_coords.x = sqrt(params.x / Atmosphere_thickness);
	
	float cos_horizon= -sqrt(params.x*(2.f * Earth_radius + params.x)) / (Earth_radius + params.x);
	if (params.y > cos_horizon)
	{
		params.y=max(params.y, cos_horizon+0.0001f);
		tex_coords.y = 0.5f*pow((params.y - cos_horizon) / (1.f - cos_horizon), 0.2f) + 0.5f;
	}
	else
	{
		params.y=min(params.y, cos_horizon-0.0001f);
		tex_coords.y= 0.5f*pow((cos_horizon - params.y) / (1.f + cos_horizon), 0.2f);
	}

	tex_coords.z = 0.5f*(atan(max(params.z, 0.f)*tan(1.26f*1.1f)) / 1.1f + 0.74f);

	return tex_coords;
}

vec2 params_to_tex_coords_for_transmittance(vec2 params)
{
	vec2 tex_coords;
	tex_coords.x = sqrt(params.x / Atmosphere_thickness);
	
	float cos_horizon= -sqrt(params.x*(2.f * Earth_radius + params.x)) / (Earth_radius + params.x);
	if (params.y > cos_horizon)
	{
		params.y=max(params.y, cos_horizon+0.0001f);
		tex_coords.y = 0.5f*pow((params.y - cos_horizon) / (1.f - cos_horizon), 0.2f) + 0.5f;
	}
	else
	{
		params.y=min(params.y, cos_horizon-0.0001f);
		tex_coords.y= 0.5f*pow((cos_horizon - params.y) / (1.f + cos_horizon), 0.2f);
	}

	return tex_coords;
}

float Rayleigh_phase(float cos_theta)
{
	return (3.f*(1.f + cos_theta*cos_theta)) / (16.f*PI);
}

float ad_hoc_Rayleigh_phase(float cos_theta)
{
	return 0.8f*(1.4f+0.5f*cos_theta);
}

float Mie_phase(float cos_theta, float g)
{
	float g_square = g*g;
	float nom = 3.f * (1.f - g_square)*(1.f + cos_theta*cos_theta);
	float denom = 2.f * (2.f + g_square)*pow(1.f + g_square - 2.f * g*cos_theta, 1.5f);
	return nom / denom;
}

float Mie_phase_Henyey_Greenstein(float cos_theta, float g)
{
	float g_square=g*g;
	float nom=1-g_square;
	float denom=4.f*PI*pow(1+g_square-2*g*cos_theta, 1.5f);
	return nom/denom;
}




void main()
{
	vec3 l=-data.light_dir;

	vec3 n=normalize(normal_in);
	
	vec3 v=normalize(pos_in-data.view_pos);
	
	vec3 refl=reflect(v, n);
	vec3 refr=refract(v, n, index_of_refr);
	
	vec4 refl_proj=data.mirrored_proj_x_view*vec4(pos_in+refl, 1.f);
	refl_proj/=refl_proj.w;
	
	vec4 refr_proj=data.proj_x_view*vec4(pos_in+refr, 1.f);
	refr_proj/=refr_proj.w;
	
	vec4 tex_coords=0.5f*vec4(refl_proj.xy, refr_proj.xy)+0.5f;
	
	//vec4 refl_color=texture(reflection_tex, tex_coords.xy);
	vec4 refl_color=vec4(0.f);
	vec4 refr_color=texture(refraction_tex, tex_coords.zw);
	
	if (refl_color.w==0.f) //data is invalid, sample from sky
	{
		//calculate reflected specular sky color
		vec3 params_refl=vec3(data.height_bias+pos_in.y, refl.y, l.y);
		vec3 tex_coords_refl=params_to_tex_coords(params_refl);
		float cos_refl_light=dot(refl, l);
		refl_color.xyz=ad_hoc_Rayleigh_phase(cos_refl_light)*texture(Rayleigh_tex, tex_coords_refl).xyz+Mie_phase_Henyey_Greenstein(cos_refl_light, 0.75f)*texture(Mie_tex, tex_coords_refl).xyz;
		refl_color.xyz=refl_color.xyz*data.irradiance;
	}
	
	v*=-1.f;
	vec3 h=normalize(l+v);
	
	float l_dot_n=max(dot(l, n), 0.0f);
	float h_dot_n=max(dot(h,n), 0.0f);
	float h_dot_l=max(dot(h,l), 0.0f);
	float v_dot_n=max(dot(v,n), 0.0f);
	
	//PBR terms	
	vec3 fresnel=fresnel_schlick(h_dot_l, water_F0);
	float ndf=NDF(pow(h_dot_n, 2.0f), water_roughness);
	float geometry=shlick_geometry_term(l_dot_n, water_roughness)*shlick_geometry_term(v_dot_n, water_roughness);
	vec3 specular=fresnel*geometry*ndf/(4.0f*l_dot_n*v_dot_n+0.001f);	
	
	vec3 lambert=(1.0f-fresnel)*water_albedo/PI;
	
	//calculate the irradiance
	vec2 params_tr=vec2(data.height_bias+pos_in.y, -data.light_dir.y);
	vec3 tr=texture(transmittance_tex, params_to_tex_coords_for_transmittance(params_tr)).xyz;
	vec3 params_sky=vec3(params_tr, l.y);
	vec3 tex_coords0=params_to_tex_coords(params_sky);
	vec3 sun_color=ad_hoc_Rayleigh_phase(1.f)*texture(Rayleigh_tex, tex_coords0).xyz+Mie_phase_Henyey_Greenstein(1.f, 0.75f)*texture(Mie_tex, tex_coords0).xyz;
	vec3 irradiance =(tr+sun_color)*data.irradiance;
	
	//calculate specular term for reflected sky
	float n_dot_r=max(0.f, dot(n, refl));
	vec3 fresnel_refl_sky=fresnel_schlick(n_dot_r, water_F0);
	float ndf_refl_sky=1.f/(PI*water_roughness*water_roughness+0.005f);//NDF(1.f, roughness);
	float geometry_refl_sky=pow(shlick_geometry_term_IBL(n_dot_r, water_roughness), 2.f);
	vec3 specular_refl_sky=fresnel_refl_sky*geometry_refl_sky*ndf_refl_sky/(4.f+pow(n_dot_r, 2.f)+0.001f);
	
	vec3 lambert_refl_sky=(1.f-fresnel_refl_sky)*water_albedo/PI;
	
	//calculate refraction term
	float minus_n_dot_refr=max(0.f, -dot(n, refr));
	vec3 fresnel_refr=fresnel_schlick(minus_n_dot_refr, water_F0);
	vec3 refracted_color=(1.f-fresnel_refr)*refr_color.xyz/PI;
	
	//calculate reflected color
	vec3 color=(specular+lambert)*irradiance*l_dot_n
		+(specular_refl_sky+lambert_refl_sky)*refl_color.xyz*n_dot_r/10.f
		+refracted_color
		+(water_albedo/PI)*data.ambient_irradiance;
		
	//adding areal perspective effect
	vec3 transm=exp(-transmittance(pos_in, data.view_pos));
	vec3 params=vec3(data.height_bias+data.view_pos.y, -v.y, l.y); 
	vec3 sky_tex_coords=params_to_tex_coords(params);
	float cos_view_sun=dot(v, l);
	vec4 Rayleigh_sampled=texture(Rayleigh_tex, sky_tex_coords);
	vec4 Mie_sampled=texture(Mie_tex, sky_tex_coords);
	vec3 sky_color=ad_hoc_Rayleigh_phase(cos_view_sun)*Rayleigh_sampled.xyz+Mie_phase_Henyey_Greenstein(cos_view_sun, 0.75f)*Mie_sampled.xyz;
	vec3 sky_scattering=sky_color*pow(min(length(pos_in-data.view_pos), Rayleigh_sampled.w)/Rayleigh_sampled.w, 0.2f);
	
	vec3 color_with_areal=transm*color+sky_scattering*data.irradiance;
	
	color_out=vec4(color_with_areal, 1.f);
	//color_out=vec4(refr_color.xyz, 1.f);
}