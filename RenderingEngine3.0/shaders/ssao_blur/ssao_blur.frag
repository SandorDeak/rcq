#version 450
#extension GL_ARB_separate_shader_objects : enable

const int KERNEL_DIM=5;

layout(set=0, binding=0) uniform sampler2D ao_tex;


layout(location=0) out vec4 ao_out;

void main()
{	
	/*float sum=0.f;
	
	for(int i=-KERNEL_DIM; i<KERNEL_DIM+1; ++i)
	{
		for(int j=-KERNEL_DIM; j<KERNEL_DIM+1; ++j)
		{
			sum+=texture(ao_tex, vec2(gl_FragCoord.x+i, gl_FragCoord.y+j)).x;
		}
	}

	ao_out.w=sum/pow(2*float(KERNEL_DIM)+1, 2.f);*/
	
	
	ao_out.w=texture(ao_tex, gl_FragCoord.xy).x;
}