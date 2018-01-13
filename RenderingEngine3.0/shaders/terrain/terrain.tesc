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
		(gl_InvocationID & 1) == 0 ? int(gl_InvocationID)-1 : 0,
		(gl_InvocationID & 1) == 0 ? 0 : int(gl_InvocationID)-2
	);
	
	ivec2 neighbour_tile_id = clamp(offset+tile_id_in[0], ivec2(0), terr.tile_count-ivec2(1));	
	
	float neighbour_mip_level=texelFetch(current_mip_levels, neighbour_tile_id.x+terr.tile_count.x*neighbour_tile_id.y).x;
	
	barrier();
	
	neighbour_mip_level=max(mip_levels_out[4], neighbour_mip_level);	
	mip_levels_out[gl_InvocationID]=neighbour_mip_level;
	float tess_level=max(terr.meter_per_tile_size_length.x, terr.meter_per_tile_size_length.y)*pow(2.f, terr.mip_level_count-neighbour_mip_level);
	gl_TessLevelOuter[gl_InvocationID]=tess_level;
	
	gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;
	
}