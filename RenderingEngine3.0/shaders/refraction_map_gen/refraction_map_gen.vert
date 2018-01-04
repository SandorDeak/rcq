#version 450
#extension GL_ARB_separate_shader_objects : enable


const vec3 vertices[4]=vec3[4](
						vec3(-1.f, 0.f,  -1.f),
						vec3(-1.f, 0.f,  1.f),
						vec3(1.f, 0.f,  1.f),
						vec3(1.f, 0.f, -1.f)
						);
	

layout(set=0, binding=0) uniform refraction_map_gen_data
{
	mat4 proj_x_view;
	vec3 view_pos_at_ground;
	float far;
} data;

layout(location=0) out vec3 surface_pos_out;

void main()
{
	surface_pos_out=vec3(0.f);//data.far*vertices[gl_VertexIndex]+vec3(data.view_pos_xz.x, 0.f, data.view_pos_xz.y);
	gl_Position=data.proj_x_view*vec4(data.view_pos_at_ground+data.far*vertices[gl_VertexIndex], 1.f);
}