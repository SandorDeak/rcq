#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(set=0, binding=0) uniform sampler2D ao_tex;


layout(location=0) out float ao_out;

void main()
{	
	ao_out=texture(ao_tex, gl_FragCoord.xy).x;
}