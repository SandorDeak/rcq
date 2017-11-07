#version 450
#extension GL_ARB_separate_shader_objects : enable

const float scale=512.f;

const vec2 vertices[4]=vec2[4](
						vec2(-1.f, -1.f),
						vec2(1.f, -1.f),
						vec2(1.f, 1.f),
						vec2(-1.f, 1.f)
						);
	
	
void main()
{
	gl_Position=vec4(scale*vertices[gl_VertexIndex].x, 0.f, scale*vertices[gl_VertexIndex].y, 1.f);
}

///////////////////////////

const float ds=1.f/255.f;
const float dt=1.f/255.f;

const uint vertex_id_bitmask=#3;

const uint square_id_x_bitmask=#000003fc;
const uint square_id_z_bitmask=#0003fc00;


layout(set=0, binding=0) terrain_drawer_data
{
	mat4 proj_x_view;	
	vec3 view_pos;
	float far;
} data;

layout(set=1, binding=0) uniform sampler2D terrain_tex;

layout(location=0) out vec3 normal_out;

void main()
{
		
	
	uint vertex_id=gl_VertexIndex & vertex_id_bitmask;
	float x=float(((square_id_x_bitmask & gl_VertexIndex) >> 2) + (vertex_id & 1));
	float z=float(((square_id_z_bitmask & gl_VertexIndex) >> 10) + ((vertex_id & 2) >> 1));
	
	float S=x*ds/(2.f*x*ds-1.f);
	float T=z*dt/(2.f*z*dt-1.f);
	
	vec3 pos=vec3((1.f-2.f*S)*data.far, 0.f, (1.f-2.f*T)*data.far);
	vec3 pos_world=vec3(pos.x+data.view_pos.x, 0.f, pos.z+data.view_pos.z);
	vec4 tex_val=texture(terrain_tex, pos_world.xz);
	normal_out=tex_val.xyz;
	pos_world.y=tex_val.w;
	gl_Position=vec4(pos_world, 1.f);
}
