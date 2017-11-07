#version 450
#extension GL_ARB_separate_shader_objects : enable

const float scale=512.f;

layout(vertices=4) out;

void main()
{
	if (gl_InvocationID==0)
	{
		gl_TessLevelInner[0]=scale;
		gl_TessLevelInner[1]=scale;
		
		gl_TessLevelOuter[0] = scale;
		gl_TessLevelOuter[1] = scale;
		gl_TessLevelOuter[2] = scale;
		gl_TessLevelOuter[3] = scale;
	}
	
	gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;
}


/////////////

layout(vertices=4) out;

layout(set=0, binding=0) terrain_drawer_data
{
	mat4 proj_x_view;	
	vec3 view_pos;
	float far;
	float tessellation_scale;
} data;

layout(location=0) in normals_in[];

out vec2 screen_space[];
out float tessellation_levels[];
out bool neg_z[];

patch out vec3 control_points_out[8];

void main()
{
	vec4 pos_proj=data.proj_x_view*gl_in[gl_InvocationID].gl_Position;
	pos_proj/=pos_proj.w;
	screen_space[gl_InvocationID]=pos_proj.xy;
	neg_z[gl_InvocationID]=(pos_proj.z<0.f);
	barrier();
	
	uint next_index=(gl_InvocationID+1) & 3;
	float screen_space_distance=distance(screen_space[gl_InvocationID], screen_space[next_index]);
	float tess=screen_space_distance*data.tessellation_scale;
	gl_TessLevelOuter[next_index]=tess;
	tessellation_levels[next_index]=tess;
	
	gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;
	
		
	vec3 dp=gl_in[next_index].gl_Position.xyz-gl_in[gl_InvocationID].gl_Position.xyz;
	control_points_out[2*gl_InvocationID]=dp-dot(normals_in[gl_InvocationID], dp)*normals_in[gl_InvocationID];
	
	uint previous_index=(gl_InvocationID+3) & 3;
	dp=gl_in[previous_index].gl_Position.xyz-gl_in[gl_InvocationID].gl_Position.xyz;
	control_points_out[2*gl_InvocationID+1]=dp-dot(normals_in[gl_InvocationID], dp)*normals_in[gl_InvocationID];
	
	barrier();
	
	if (gl_InvocationID==0)
	{
		if (neg_z[0] && neg_z[1] && neg_z[2] && neg_z[3])
		{
			gl_TessLevelInner[0]=0.f;
			gl_TessLevelInner[1]=0.f;
		}
		else
		{
			gl_TessLevelInner[0]=max(tessellation_levels[1], tessellation_levels[3]);
			gl_TessLevelInner[1]=max(tessellation_levels[0], tessellation_levels[2]);
		}
	}
}