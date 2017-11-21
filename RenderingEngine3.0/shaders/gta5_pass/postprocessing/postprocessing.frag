#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(set=0, binding=0) uniform sampler2D preimage_tex;

layout(location=0) out vec4 color_out;
layout(location=1) out vec4 prev_color_out;

void main()
{	
	vec4 color=texture(preimage_tex, gl_FragCoord.xy);
	
	color_out=pow(color, vec4(0.45f));
	
	prev_color_out=color;
	
}