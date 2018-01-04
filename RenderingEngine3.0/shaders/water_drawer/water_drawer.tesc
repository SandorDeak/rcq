#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(vertices=4) out;

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

float screen_space_dist(vec4 a, vec4 b)
{
	a=data.proj_x_view*a;
	b=data.proj_x_view*b;
	
	a/=a.w;
	b/=b.w;
	
	return distance(data.half_resolution*a.xy, data.half_resolution*b.xy);
}

layout(location=0) patch out float outer_tess[4];

void main()
{
	float dist;
	if (gl_InvocationID==0)
		dist=screen_space_dist(gl_in[0].gl_Position, gl_in[2].gl_Position);
	if (gl_InvocationID==1)
		dist=screen_space_dist(gl_in[0].gl_Position, gl_in[1].gl_Position);
	if (gl_InvocationID==2)
		dist=screen_space_dist(gl_in[1].gl_Position, gl_in[3].gl_Position);
	if (gl_InvocationID==3)
		dist=screen_space_dist(gl_in[2].gl_Position, gl_in[3].gl_Position);
		
	gl_TessLevelOuter[gl_InvocationID]=max(1.f, dist/8.f);
	outer_tess[gl_InvocationID]=max(1.f, dist/16.f);
	gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;
	
	barrier();
	
	if (gl_InvocationID==0 || gl_InvocationID==1)
	{
		gl_TessLevelInner[(gl_InvocationID+1) & 1]=max(outer_tess[gl_InvocationID], outer_tess[gl_InvocationID+2]);
	}
}