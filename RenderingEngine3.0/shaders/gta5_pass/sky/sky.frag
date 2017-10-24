#version 450
#extension GL_ARB_separate_shader_objects : enable

const float Atmosphere_thickness=80000;
const float Earth_radius=6371000;

layout(set=0, binding=0) uniform sky_data
{
	mat4 proj_x_view_at_origin;
	vec3 light_dir;
	float height;
	vec3 irradiance;
} data;

layout(set=1, binding=0) uniform sampler3D sky_tex;

layout(location=0) in vec3 view_dir_in;

layout(location=0) out vec4 color_out;

vec3 params_to_tex_coords(vec3 params)
{
	vec3 tex_coords;
	tex_coords.x = sqrt(params.x / Atmosphere_thickness);
	
	float cos_horizon= -sqrt(params.x*(2.f * Earth_radius + params.x)) / (Earth_radius + params.x);
	if (params.y > cos_horizon)
	{
		tex_coords.y = 0.5f*pow((params.y - cos_horizon) / (1.f - cos_horizon), 0.2f) + 0.5f;
	}
	else
	{
		tex_coords.y= 0.5f*pow((cos_horizon - params.y) / (1.f + cos_horizon), 0.2f);
	}

	tex_coords.z = 0.5f*(atan(max(params.z, -0.1975f)*tan(1.26f*1.1f)) / 1.1f + 0.74f);

	return tex_coords;
}

void main()
{
	vec3 v=normalize(view_dir_in);
	float cos_view_zenit=v.y;
	float cos_sun_zenit=-data.light_dir.y;
	vec3 tex_coords=params_to_tex_coords(vec3(data.height, cos_view_zenit, cos_sun_zenit));	
	vec3 color=data.irradiance*texture(sky_tex, tex_coords).xyz;
	color_out=vec4(color, 1.f);
}