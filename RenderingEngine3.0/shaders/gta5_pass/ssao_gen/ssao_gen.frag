#version 450
#extension GL_ARB_separate_shader_objects : enable

const int KERNEL_DIM=5;

layout(set=0, binding=0) uniform sampler2D pos_tex;

layout(input_attachment_index=0, set=0, binding=1) uniform subpassInput normal_in;

layout(location=0) out float ao_out;

void main()
{
	vec3 n=subpassLoad(normal_in).xyz;
	vec3 p0=texture(pos_tex, gl_FragCoord.xy).xyz;
	uint sum=0;
	float bias=0.1f;
	
	for(int i=-KERNEL_DIM; i<KERNEL_DIM+1; ++i)
	{
		for(int j=-KERNEL_DIM; j<KERNEL_DIM+1; ++j)
		{
			vec3 p=texture(pos_tex, vec2(gl_FragCoord.x+i, gl_FragCoord.y+j)).xyz;
			if (dot(n, p-p0)<bias)
			{
				++sum;
			}
		}
	}
	
	ao_out=(float(sum))/(float(KERNEL_DIM*KERNEL_DIM));
}