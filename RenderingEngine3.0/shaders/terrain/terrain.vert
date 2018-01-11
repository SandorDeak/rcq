#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(set=1, binding=0) uniform terrain_buffer
{
	vec2 terrain_size_in_meters;
	vec2 meter_per_tile_size_length;
	ivec2 tile_count;
	float mip_level_count;
	float height_scale;
} terr;


layout(location=0) out ivec2 tile_id_out;

void main()
{	
	uint vertex_id=gl_VertexIndex & 3;
	
	int x=int((gl_VertexIndex>>2) + (vertex_id & 1));
	int y=int(gl_InstanceIndex + (vertex_id >> 1));
	 
	tile_id_out=ivec2(int(gl_VertexIndex>>2), int(gl_InstanceIndex));
	//tile_id_out=x+y*terr.tile_count.x;
	//mip_level_out=texelFetch(current_mip_levels, tile_id).x;
	gl_Position=vec4(terr.meter_per_tile_size_length.x*float(x), 0.f, terr.meter_per_tile_size_length.y*float(y), 1.f);
}
