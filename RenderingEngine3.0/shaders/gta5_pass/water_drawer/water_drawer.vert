#version 450
#extension GL_ARB_separate_shader_objects : enable

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


void main()
{	
	uint vertex_id=gl_VertexIndex & 3;
	
	/*if (vertex_id>>1==0)
	{
		vertex_id=1-vertex_id;
	}*/
	
	vec2 pos=vec2(
		float((gl_VertexIndex>>2) + (vertex_id & 1)),
		float(gl_InstanceIndex + (vertex_id >> 1))
	);
	pos=pos*data.tile_size_in_meter+data.tile_offset;

	gl_Position=vec4(pos.x, 0.f, pos.y, 1.f);
}
