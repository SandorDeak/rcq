#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(quads, equal_spacing) in;

layout(set=0, binding=0) uniform water_drawer_data
{
	mat4 proj_x_view;
	vec3 view_pos;
	vec3 light_dir;
	vec2 tile_offset;
	vec2 tile_size_in_meter;
	vec2 half_resolution;
} data;

layout(set=1, binding=0) uniform sampler2DArray water_tex; //0: height, 1: grad

layout(location=0) out vec3 color_out;

layout(location=0) patch in float outer_tess[4];

void main()
{
	vec4 mid1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 mid2 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.x);
	vec4 pos = mix(mid1, mid2, gl_TessCoord.y);
	
	float height=texture(water_tex, vec3(gl_TessCoord.xy, 0.f)).x;
	vec2 grad=texture(water_tex, vec3(gl_TessCoord.xy, 1.f)).xz;
	
	pos.y=height;
	
	gl_Position=data.proj_x_view*pos;
	
	vec3 n=normalize(vec3(-grad.x, 1.f, -grad.y));
	
	color_out=vec3(gl_TessCoord.xy, 0.f);//vec3(max(0.f, dot(n, -data.light_dir)));
}