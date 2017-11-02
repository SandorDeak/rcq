#version 450
#extension GL_ARB_separate_shader_objects : enable

const float scale=256.f;

layout(vertices=4) out;

void main()
{
	if (gl_InvocationID==0)
	{
		gl_TessLevelInner[0]=scale;
		gl_TessLevelInner[1]=scale;
		
		gl_TessLevelOuter[0] = scale;
		gl_TessLevelOuter[1] = scale;
		gl_TessLevelOuter[2] = scale;
		gl_TessLevelOuter[3] = scale;
	}
	
	gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;
}