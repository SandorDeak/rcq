#version 450
#extension GL_ARB_separate_shader_objects : enable

#define PI 3.1415926535897f

const float Atmosphere_thickness=80000;
const float Earth_radius=6371000;

layout(set=0, binding=0) uniform sky_data
{
	mat4 proj_x_view_at_origin;
	vec3 light_dir;
	float height;
	vec3 irradiance;
} data;

layout(set=1, binding=0) uniform sampler3D Rayleigh_tex;
layout(set=1, binding=1) uniform sampler3D Mie_tex;
layout(set=1, binding=2) uniform sampler2D transmittance_tex;

layout(location=0) in vec3 view_dir_in;

layout(location=0) out vec4 color_out;

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
	vec3 v=normalize(view_dir_in);
	float cos_view_zenit=v.y;
	float cos_sun_zenit=-data.light_dir.y;
	vec3 tex_coords=params_to_tex_coords(vec3(data.height, cos_view_zenit, cos_sun_zenit));	
	float cos_view_sun=dot(v, -normalize(data.light_dir));
	vec3 color=data.irradiance*(ad_hoc_Rayleigh_phase(cos_view_sun)*texture(Rayleigh_tex, tex_coords).xyz+Mie_phase_Henyey_Greenstein(cos_view_sun, 0.75f)*texture(Mie_tex, tex_coords).xyz);
	color_out=vec4(color, 1.f);
	//color_out=20.f*texture(transmittance_tex, tex_coords.xy);
	
}