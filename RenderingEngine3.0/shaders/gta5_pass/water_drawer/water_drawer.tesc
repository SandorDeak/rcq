#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(vertices=4) out;

layout(set=0, binding=0) uniform water_drawer_data
{
	mat4 proj_x_view;
	vec2 tile_offset;
	vec2 tile_size_in_meter;
	vec3 view_pos;
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
	/*if (gl_InvocationID==0)
	{
		vec3 tile_center=vec3(0.f);
		for(uint i=0; i<4; ++i)
			tile_center+=gl_in[i].gl_Position.xyz;
		tile_center*=0.25.f;
		float dist=distance(tile_center, data.view_pos);
		float scale=1.f;
		
	}*/
	
	float dist=screen_space_dist(gl_in[gl_InvocationID].gl_Position, gl_in[(gl_InvocationID+1) & 3].gl_Position);
	gl_TessLevelOuter[gl_InvocationID]=dist/16.f;
	
	/*barrier();
	
	if (gl_InvocationID==0 || gl_InvocationID==1)
	{
		gl_TessLevelInner[gl_InvocationID]=max(outer_tess[gl_InvocationID], outer_tess[gl_InvocationID+2]);
	}*/
}