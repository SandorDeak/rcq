#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint MAX_TILE_COUNT=128;//2048;
const uint MAX_TILE_COUNT_LOG2=11;

layout(vertices=4) out;

layout(set=0, binding=1) uniform samplerBuffer current_mip_levels;

layout(set=1, binding=0) uniform terrain_buffer
{
	vec2 terrain_size_in_meters;
	vec2 meter_per_tile_size_length;
	ivec2 tile_count;
	float mip_level_count;
	float height_scale;
} terr;


layout(location=0) in ivec2 tile_id_in[];

layout(location=0) patch out float mip_levels_out[5];

void main()
{

	if (gl_InvocationID==3)
	{
		int tile_id=tile_id_in[3].x+tile_id_in[3].y*terr.tile_count.x;
		float mip_level=texelFetch(current_mip_levels, tile_id).x;
		float tess_level=max(terr.meter_per_tile_size_length.x, terr.meter_per_tile_size_length.y)*pow(2.f, terr.mip_level_count-mip_level);
		gl_TessLevelInner[0]=tess_level;
		gl_TessLevelInner[1]=tess_level;
		
		mip_levels_out[4]=mip_level;
	}
	
	ivec2 offset=ivec2
	(
		int(gl_InvocationID & 1)*2-1,
		int(gl_InvocationID >> 1)*2-1
	);
	
	ivec2 neighbour_tile_id = clamp(offset+tile_id_in[0], ivec2(0), terr.tile_count-ivec2(1));
	
	
	float neighbour_mip_level=mip_levels_out[gl_InvocationID]=texelFetch(current_mip_levels, neighbour_tile_id.x+terr.tile_count.x*neighbour_tile_id.y).x;
	mip_levels_out[gl_InvocationID]=neighbour_mip_level;
	
	
	barrier();
	
	neighbour_mip_level=max(mip_levels_out[4], neighbour_mip_level); 
	float tess_level=max(terr.meter_per_tile_size_length.x, terr.meter_per_tile_size_length.y)*pow(2.f, terr.mip_level_count-neighbour_mip_level);
	gl_TessLevelOuter[gl_InvocationID]=tess_level;
	
	gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;
	
}
	
	
////////////////

	/*if (gl_InvocationID==0)
	{
		float tess_level=max(terr.meter_per_tile_size_length.x, terr.meter_per_tile_size_length.y)*pow(2.f, terr.mip_level_count-mip_level_in[0]);
		mip_level_out=mip_level_in[0];
		gl_TessLevelInner[0]=tess_level;
		gl_TessLevelInner[1]=tess_level;
		
		gl_TessLevelOuter[0] = tess_level;
		gl_TessLevelOuter[1] = tess_level;
		gl_TessLevelOuter[2] = tess_level;
		gl_TessLevelOuter[3] = tess_level;
	}*/
	//gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;
		
	//gl_out[gl_InvocationID].gl_Position=vec4(10.f*gl_in[gl_InvocationID].gl_Position.xyz, 1.f);
//}





/*void main()
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
}*/