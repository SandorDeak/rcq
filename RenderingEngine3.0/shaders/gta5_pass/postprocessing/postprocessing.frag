#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(set=0, binding=0) uniform sampler2D preimage_tex;


layout(location=0) out vec4 color_out;

void main()
{	
	color_out=texture(preimage_tex, gl_FragCoord.xy);
}