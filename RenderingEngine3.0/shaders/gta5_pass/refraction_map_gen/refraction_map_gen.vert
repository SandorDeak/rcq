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
	mat4 proj_x_view_at_origin;
	float far;
} data;
	

	

void main()
{
	gl_Position=vec4(vertices[gl_VertexIndex].x*data.far, vertices[gl_VertexIndex].z*data.far, 0.f, 1.f);//data.proj_x_view_at_origin*vec4(data.far*vertices[gl_VertexIndex], 1.f);
}