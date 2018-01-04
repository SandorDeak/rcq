#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0) uniform sky_data
{
	mat4 proj_x_view_at_origin;
	vec3 light_dir;
	float height;
	vec3 irradiance;
} data;

const vec3 vertices[8]=vec3[8](
	vec3(-1.f, -1.f, -1.f),
	vec3(-1.f, -1.f, 1.f),
	vec3(-1.f, 1.f, -1.f),
	vec3(-1.f, 1.f, 1.f),
	vec3(1.f, 1.f, -1.f),
	vec3(1.f, 1.f, 1.f),
	vec3(1.f, -1.f, -1.f),
	vec3(1.f, -1.f, 1.f)
	);
	
layout(points) in;
layout(triangle_strip, max_vertices=18) out;

layout(location=0) out vec3 view_dir_out;
	
void main()
{
	vec4 current_vertices[8];
	
	//sides
	for(uint i=0; i<8; ++i)
	{
		current_vertices[i]=data.proj_x_view_at_origin*vec4(vertices[i], 1.0);
		current_vertices[i]=current_vertices[i].xyww;
	}
	
	for (uint i=0; i<8; ++i)
	{
		gl_Position=current_vertices[i];
		view_dir_out=vertices[i];
		EmitVertex();
	}
	for (uint i=0; i<2; ++i)
	{
		gl_Position=current_vertices[i];
		view_dir_out=vertices[i];
		EmitVertex();
	}	
	EndPrimitive();
	
	//top
	gl_Position=current_vertices[0];
	view_dir_out=vertices[0];
	EmitVertex();
	gl_Position=current_vertices[2];
	view_dir_out=vertices[2];
	EmitVertex();
	gl_Position=current_vertices[6];
	view_dir_out=vertices[6];
	EmitVertex();
	gl_Position=current_vertices[4];
	view_dir_out=vertices[4];
	EmitVertex();
	EndPrimitive();
	
	//bottom
	gl_Position=current_vertices[1];
	view_dir_out=vertices[1];
	EmitVertex();
	gl_Position=current_vertices[7];
	view_dir_out=vertices[7];
	EmitVertex();
	gl_Position=current_vertices[3];
	view_dir_out=vertices[3];
	EmitVertex();
	gl_Position=current_vertices[5];
	view_dir_out=vertices[5];
	EmitVertex();
	EndPrimitive();
}