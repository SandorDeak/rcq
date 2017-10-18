#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(set=0, binding=0) uniform sampler2D ao_tex;


layout(location=0) out vec4 ao_out;

void main()
{	
	ao_out.w=texture(ao_tex, gl_FragCoord.xy).x;
}