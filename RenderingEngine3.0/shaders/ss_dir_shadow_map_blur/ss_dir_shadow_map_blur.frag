#version 450
#extension GL_ARB_separate_shader_objects : enable

const int KERNEL_DIM=5;

layout(set=0, binding=0) uniform sampler2D pos_tex;
layout(set=0, binding=1) uniform sampler2D shadow_tex;



layout(set=0, binding=2) uniform sampler2D normal_in;

layout(location=0) out vec4 shadow_out;


void main()
{
	vec3 n=texture(normal_in, gl_FragCoord.xy).xyz;
	vec3 p0=texture(pos_tex, gl_FragCoord.xy).xyz;
	float bias=0.2f;
	uint blur_count=0;
	float sum=0.f;
	for(int i=-KERNEL_DIM; i<KERNEL_DIM+1; ++i)
	{
		for(int j=-KERNEL_DIM; j<KERNEL_DIM+1; ++j)
		{
			vec2 tex_coord=vec2(gl_FragCoord.x+i, gl_FragCoord.y+j);
			vec3 p=texture(pos_tex, tex_coord).xyz;
			if (abs(dot(n,p-p0))<bias)
			{
				sum+=texture(shadow_tex, tex_coord).x;
				++blur_count;
			}
		}
	}
	
	shadow_out.w=sum/float(blur_count);
	
	//shadow_out.w=texture(shadow_tex, gl_FragCoord.xy).x;
	
}