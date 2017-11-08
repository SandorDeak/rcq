#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint MAX_REQUEST_COUNT=256;
const uint MAX_TILE_COUNT=2048;
const uint MAX_TILE_COUNT_LOG2=11;

const uint vertex_id_bitmask=3;

const uint tile_id_x_bitmask=(MAX_TILE_COUNT-1)<<2;
const uint tile_id_z_bitmask=(MAX_TILE_COUNT-1)<<(2+MAX_TILE_COUNT_LOG2);

layout(set=1, binding=0) uniform terrain_buffer
{
	float current_mip_levels[MAX_TILE_COUNT][MAX_TILE_COUNT];
	float mip_level_count;
	float scale; //meter per tile side length
	float height_scale;
	vec2 terrain_size;
	uint request_count;
	uint requests[MAX_REQUEST_COUNT];
} terr;


layout(location=0) out float mip_level_out;

void main()
{	
	uint vertex_id=gl_VertexIndex & vertex_id_bitmask;
	
	uint x=((tile_id_x_bitmask & gl_VertexIndex) >> 2) + (vertex_id & 1);
	uint z=((tile_id_z_bitmask & gl_VertexIndex) >> (2+MAX_TILE_COUNT_LOG2)) + ((vertex_id & 2) >> 1);
	
	mip_level_out=terr.current_mip_levels[x][z];
	gl_Position=vec4(float(x), 0.f, float(z), 1.f);
}
