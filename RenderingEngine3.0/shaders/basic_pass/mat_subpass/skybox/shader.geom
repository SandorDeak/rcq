#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0) uniform camera_data
{
	mat4 proj_x_view;
	vec3 pos;
	uint padding0;
} cam;

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

layout(location=0) out vec3 tex_coord;
	
void main()
{
	vec4 current_vertices[8];
	
	//sides
	for(uint i=0; i<8; ++i)
	{
		current_vertices[i]=cam.proj_x_view*vec4(vertices[i]+cam.pos, 1.0);
		current_vertices[i]=current_vertices[i].xyww;
	}
	
	for (uint i=0; i<8; ++i)
	{
		gl_Position=current_vertices[i];
		tex_coord=vertices[i];
		EmitVertex();
	}
	for (uint i=0; i<2; ++i)
	{
		gl_Position=current_vertices[i];
		tex_coord=vertices[i];
		EmitVertex();
	}	
	EndPrimitive();
	
	//top
	gl_Position=current_vertices[0];
	tex_coord=vertices[0];
	EmitVertex();
	gl_Position=current_vertices[2];
	tex_coord=vertices[2];
	EmitVertex();
	gl_Position=current_vertices[6];
	tex_coord=vertices[6];
	EmitVertex();
	gl_Position=current_vertices[4];
	tex_coord=vertices[4];
	EmitVertex();
	EndPrimitive();
	
	//bottom
	gl_Position=current_vertices[1];
	tex_coord=vertices[1];
	EmitVertex();
	gl_Position=current_vertices[7];
	tex_coord=vertices[7];
	EmitVertex();
	gl_Position=current_vertices[3];
	tex_coord=vertices[3];
	EmitVertex();
	gl_Position=current_vertices[5];
	tex_coord=vertices[5];
	EmitVertex();
	EndPrimitive();
}