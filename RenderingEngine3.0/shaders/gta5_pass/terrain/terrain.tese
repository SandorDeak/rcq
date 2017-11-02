#version 450
#extension GL_ARB_separate_shader_objects : enable

const vec3 water_color=vec3(0.f, 0.f, 1.f);
const vec3 sand_color=vec3(1.f, 1.f, 0.f);
const vec3 rock_color=vec3(0.5f, 0.5f, 0.5f);

const float height_scale=0.1f;


layout(quads, equal_spacing) in;

layout(set=0, binding=0) uniform terrain_data
{
	mat4 proj_x_view;
} data;

layout(set=1, binding=0) uniform sampler2D terrain_tex;

layout(location=0) out vec3 color_out;

void main()
{
	vec4 mid1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 mid2 = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x);
	vec4 pos = mix(mid1, mid2, gl_TessCoord.y);

	vec4 tex_val=texture(terrain_tex, gl_TessCoord.xy);
	
	float height=tex_val.x;
	float water=tex_val.y;
	float sand=tex_val.z;
	
	pos.y=height*height_scale;
	color_out=water*water_color+sand*sand_color+max(1.f-water-sand, 0.f)*rock_color;
	
	gl_Position=data.proj_x_view*pos;
}