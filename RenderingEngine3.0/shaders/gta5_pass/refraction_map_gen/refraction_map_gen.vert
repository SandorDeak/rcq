#version 450
#extension GL_ARB_separate_shader_objects : enable


const vec3 vertices[4]=vec3[4](
						vec2(-1.f, 0.f,  -1.f),
						vec2(-1.f, 0.f,  1.f),
						vec2(1.f, 0.f,  1.f),
						vec2(1.f, 0.f, -1.f)
						);
	

layout(set=0, binding=0) uniform refraction_map_gen_data
{
	mat4 proj_x_view;
	float far;
} data;
	

	

void main()
{
	gl_Position=data.proj_x_view*vec4(data.far*vertices[gl_VertexIndex], 1.f);
}