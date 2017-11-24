#version 450
#extension GL_ARB_separate_shader_objects : enable

const vec2 vertices[4]=vec2[4](
	vec2(-1.f, -1.f),
	vec2(-1.f, 1.f),
	vec2(1.f, 1.f),
	vec2(1.f, -1.f)
);
	
layout(location=0) out vec2 tex_coord;

void main()
{
	tex_coord=0.5f*vertices[gl_VertexIndex]+0.5f;
	gl_Position=vec4(vertices[gl_VertexIndex], 0.f, 1.f);
}