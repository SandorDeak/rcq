#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint MAX_TILE_COUNT=128;//2048;
const uint MAX_TILE_COUNT_LOG2=11;

layout(quads, equal_spacing) in;

layout(set=0, binding=0) uniform terrain_data
{
	mat4 proj_x_view;
	vec3 light_dir;
} data;

layout(set=1, binding=0) uniform terrain_buffer
{
	vec2 terrain_size_in_meters;
	vec2 meter_per_tile_size_length;
	ivec2 tile_count;
	float mip_level_count;
	float height_scale;
} terr;

layout(set=1, binding=1) uniform sampler2D terrain_tex;

layout(location=0) patch in float mip_level_in;

layout(location=0) out vec3 color_out;

void main()
{
	vec4 mid1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 mid2 = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x);
	vec4 pos = mix(mid1, mid2, gl_TessCoord.y);
	
	vec2 tex_coords=pos.xz/terr.terrain_size_in_meters;
	
	/*float level=textureQueryLod(terrain_tex, tex_coords).x;
	level=max(level, mip_level_in);	*/
	vec3 tex_val=textureLod(terrain_tex, tex_coords, mip_level_in).xyz*terr.height_scale;
	//pos.y=tex_val.y;
	vec3 n=normalize(vec3(tex_val.x, 1.f, tex_val.z));
	
	//color_out=vec3(max(dot(n, -data.light_dir), 0.f));
	color_out=vec3(pos.y);
	
	gl_Position=data.proj_x_view*pos;
}


////////////////////////////

/*layout(set=0, binding=0) terrain_drawer_data
{
	mat4 proj_x_view;	
	vec3 view_pos;
	float far;
	float tessellation_scale;
} data;

patch in vec3 control_points_in[8];

layout(location=0) out color_out;


void main()
{
	float u=gl_TessCoord.x;
	float v=gl_TessCoord.y;
	
	vec3 mid1=gl_in[0].gl_Position.xyz*pow(u, 3.f)+control_points_in[0]*3.f*u*u*(1.f-u)+control_points_in[3]*3.f*u*(1.f-u)*(1.f-u)+gl_in[1].gl_Position.xyz*pow(1.f-u, 3.f);
	vec3 mid1=gl_in[0].gl_Position.xyz*pow(u, 3.f)+control_points_in[0]*3.f*u*u*(1.f-u)+control_points_in[3]*3.f*u*(1.f-u)*(1.f-u)+gl_in[1].gl_Position.xyz*pow(1.f-u, 3.f);
}*/


























