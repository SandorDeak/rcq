#version 450
#extension GL_ARB_separate_shader_objects : enable

#define n 0.1f
#define f 1000.f

const mat4 proj[6]= mat4[6](	
	mat4(
		vec4(0.f, 0.f, -f/(n-f), 1.f),
		vec4(0.f, -1.f, 0.f, 0.f),
		vec4(-1.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, (f*n)/(n-f), 0.f)
		),
	mat4(
		vec4(0.f, 0.f, f/(n-f), -1.f),
		vec4(0.f, -1.f, 0.f, 0.f),
		vec4(1.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, (f*n)/(n-f), 0.f)
		),
	mat4(
		vec4(1.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, -f/(n-f), 1.f),
		vec4(0.f, 1.f, 0.f, 0.f),
		vec4(0.f, 0.f, (f*n)/(n-f), 0.f)
		),
	mat4(
		vec4(1.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, f/(n-f), -1.f),
		vec4(0.f, -1.f, 0.f, 0.f),
		vec4(0.f, 0.f, (f*n)/(n-f), 0.f)
		),
	mat4(
		vec4(1.f, 0.f, 0.f, 0.f),
		vec4(0.f, -1.f, 0.f, 0.f),
		vec4(0.f, 0.f, -f/(n-f), 1.f),
		vec4(0.f, 0.f, (f*n)/(n-f), 0.f)
		),
	mat4(
		vec4(-1.f, 0.f, 0.f, 0.f),
		vec4(0.f, -1.f, 0.f, 0.f),
		vec4(0.f, 0.f, f/(n-f), -1.f),
		vec4(0.f, 0.f, (f*n)/(n-f), 0.f)
		)
	);

layout(invocations=6) in;

layout(triangles) in;

layout(location=0) in vec3 normal_in[3];
layout(location=1) in vec2 tex_coord_in[3];



layout(location=0) out vec3 normal_out;
layout(location=1) out vec2 tex_coord_out;

layout(triangle_strip, max_vertices=3) out;

void main()
{
	for (uint i=0; i<3; ++i)
	{
		gl_Layer=gl_InvocationID;
		gl_Position=proj[gl_InvocationID]*vec4(gl_in[i].gl_Position.xyz, 1.f);//gl_in[i].gl_Position;//proj[gl_InvocationID]*gl_in[i].gl_Position;
		normal_out=normal_in[i];
		tex_coord_out=tex_coord_in[i];
		EmitVertex();
	}
	
	EndPrimitive();
}