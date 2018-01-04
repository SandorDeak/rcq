#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=1) uniform sampler2D preimage_in;

layout(location=0) in vec3 surface_pos_in;

layout(location=0) out vec4 color_out;

//should work only if there is value in preimage (use stencil buffer!!!)

void main()
{
	color_out=texture(preimage_in, gl_FragCoord.xy);
}