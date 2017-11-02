#version 450
#extension GL_ARB_separate_shader_objects : enable

const float scale=256.f;

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
