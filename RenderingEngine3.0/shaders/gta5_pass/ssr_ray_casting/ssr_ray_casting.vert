#version 450
#extension GL_ARB_separate_shader_objects : enable


void main()
{
	gl_Position=vec4(float(gl_VertexIndex)+0.5f, float(gl_InstanceIndex)+0.5f, 0.f, 0.f);
}