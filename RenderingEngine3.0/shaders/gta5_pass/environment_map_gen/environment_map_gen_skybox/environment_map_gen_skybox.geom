#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(invocations=6) in;

layout(triangles) in;

layout(location=0) out vec3 tex_coord_out;

layout(triangle_strip, max_vertices=3) out;

void main()
{
	for (uint i=0; i<3; ++i)
	{
		gl_Layer=gl_InvocationID;
		gl_Position=gl_in[i].gl_Position;
		tex_coord_out=vec3(gl_in[i].gl_Position.xy, float(i));
		EmitVertex();
	}
	
	EndPrimitive();
}