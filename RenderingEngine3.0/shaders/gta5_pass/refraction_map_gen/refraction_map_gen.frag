#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=1) uniform preimage_tex;

layout(location=0) out vec4 color_out;


//should work only if there is value in preimage (use stencil buffer!!!)

void main()
{
	vec4 color_in=texture(preimage_tex, gl_FragCoord.xy);
}