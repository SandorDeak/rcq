#version 450
#extension GL_ARB_separate_shader_objects : enable

#define PI 3.1415926535897f

const float Atmosphere_thickness=80000;
const float Earth_radius=6371000;


layout(set=0, binding=0) uniform sun_data
{
	mat4 proj_x_view_at_origin;
	vec3 light_dir;
	uint padding0;
	vec3 helper_dir;
	float height;
	vec3 irradiance;
} data;

layout(set=1, binding=0) uniform sampler3D Rayleigh_tex;
layout(set=1, binding=1) uniform sampler3D Mie_tex;
layout(set=1, binding=2) uniform sampler2D transmittance_tex;

layout(location=0) in float distance_from_center_in;
layout(location=1) in vec3 view_dir_in;

layout(location=0) out vec4 color_out;

vec2 params_to_tex_coords(vec2 params)
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

void main()
{
	vec2 params=vec2(data.height, -normalize(view_dir_in).y);
	vec3 tr=texture(transmittance_tex, params_to_tex_coords(params)).xyz;
	color_out=vec4(cos(distance_from_center_in)*tr*data.irradiance, 1.f);
}