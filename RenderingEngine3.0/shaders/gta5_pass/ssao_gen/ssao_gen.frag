#version 450
#extension GL_ARB_separate_shader_objects : enable

const int KERNEL_DIM=5;

layout(set=0, binding=0) uniform sampler2D pos_tex;

layout(set=0, binding=1) uniform sampler2D normal_in;


void main()
{
	vec3 n=texture(normal_in, gl_FragCoord.xy).xyz;
	vec3 p0=texture(pos_tex, gl_FragCoord.xy).xyz;
	uint sum=0;
	float bias=0.001f;
	
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
	
	gl_FragDepth=float(sum)/pow(2*float(KERNEL_DIM)+1, 2.f);
}