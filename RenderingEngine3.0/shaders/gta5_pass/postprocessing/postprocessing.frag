#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(set=0, binding=0) uniform sampler2D preimage_tex;
layout(set=0, binding=1) uniform sampler2DArray bloom_blur_tex;

layout(location=0) in vec2 tex_coord;

layout(location=0) out vec4 color_out;
layout(location=1) out vec4 prev_color_out;

float A = 0.15f;
float B = 0.50f;
float C = 0.10f;
float D = 0.20f;
float E = 0.02f;
float F = 0.30f;
float W = 11.2f;

vec3 Uncharted2Tonemap(vec3 x)
{
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}



void main()
{	
	vec3 preim_color=texture(preimage_tex, gl_FragCoord.xy).xyz;
	vec3 bloom_color=texture(bloom_blur_tex, vec3(tex_coord, 1.f)).xyz;
	
	float ExposureBias =2.0f;
	vec3 curr = Uncharted2Tonemap(ExposureBias*(preim_color+bloom_color));

	vec3 whiteScale = 1.0f/Uncharted2Tonemap(vec3(W));
	vec3 color = curr*whiteScale;
	
	
	color_out=vec4(pow(color, vec3(0.45f)), 1.f);
	
	prev_color_out=vec4(preim_color, 1.f);
	
}