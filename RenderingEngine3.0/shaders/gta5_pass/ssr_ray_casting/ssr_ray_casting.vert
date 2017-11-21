#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location=0) out ivec2 fragment_id_out;

void main()
{
	fragment_id_out=ivec2(gl_VertexIndex, gl_InstanceIndex);
	gl_Position=vec4(gl_VertexIndex, gl_InstanceIndex, 0.f, 0.f);
}