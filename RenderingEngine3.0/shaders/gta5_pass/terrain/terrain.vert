#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint MAX_TILE_COUNT=128;//2048;
const uint MAX_TILE_COUNT_LOG2=7;

const uint vertex_id_bitmask=3;

const uint tile_id_x_bitmask=(MAX_TILE_COUNT-1)<<2;
const uint tile_id_z_bitmask=(MAX_TILE_COUNT-1)<<(2+MAX_TILE_COUNT_LOG2);

layout(set=1, binding=0) uniform terrain_buffer
{
	vec2 terrain_size_in_meters;
	vec2 meter_per_tile_size_length;
	ivec2 tile_count;
	float mip_level_count;
	float height_scale;
} terr;

layout(set=1, binding=2) uniform samplerBuffer current_mip_levels;


layout(location=0) out float mip_level_out;

void main()
{	
	uint vertex_id=gl_VertexIndex & vertex_id_bitmask;
	//int x=int(((tile_id_x_bitmask & gl_VertexIndex) >> 2) + (vertex_id & 1));
	//int y=int(((tile_id_z_bitmask & gl_VertexIndex) >> (2+MAX_TILE_COUNT_LOG2)) + ((vertex_id & 2) >> 1));
	
	int x=int(((gl_VertexIndex>>2) & (MAX_TILE_COUNT-1))+ (vertex_id & 1));
	int y=int(((gl_VertexIndex>>(MAX_TILE_COUNT_LOG2+2)) & (MAX_TILE_COUNT-1)) + (vertex_id>>1));
	
	
	int tile_id=x+y*terr.tile_count.x;
	//mip_level_out=imageLoad(current_mip_levels, tile_id).x;
	mip_level_out=texelFetch(current_mip_levels, tile_id).x;
	gl_Position=vec4(float(x), 0.f, float(y), 1.f);
}
