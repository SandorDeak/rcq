#version 450
#extension GL_ARB_separate_shader_objects : enable


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
	

layout(points) in;

layout(triangle_strip, max_vertices=18*6) out;

layout(set=0, binding=0) uniform environment_map_gen_skybox_data
{
	mat4 view;
} data;

layout(location=0) out vec3 tex_coord_out;

void main()
{
	mat3 view3=mat3(data.view);
	vec3 vertices_view[8];
	for(uint i=0; i<8; ++i)
	{
		vertices_view[i]=view3*vertices[i];
	}
	
	vec4 current_vertices[8];
	for(int i=0; i<6; ++i)
	{
		for(uint j=0; j<8; ++j)
		{
			current_vertices[j]=proj[i]*vec4(vertices_view[j], 1.f);
			current_vertices[i]=current_vertices[i].xyww;
		}
		
		//sides
		for (uint j=0; j<8; ++i)
		{
			gl_Layer=i;
			gl_Position=current_vertices[j];
			tex_coord_out=vertices[j];
			EmitVertex();
		}
		for (uint j=0; j<2; ++i)
		{
			gl_Layer=i;
			gl_Position=current_vertices[j];
			tex_coord_out=vertices[j];
			EmitVertex();
		}	
		EndPrimitive();
	
		//top
		gl_Layer=i;
		gl_Position=current_vertices[0];
		tex_coord_out=vertices[0];
		EmitVertex();
		gl_Layer=i;
		gl_Position=current_vertices[2];
		tex_coord_out=vertices[2];
		EmitVertex();
		gl_Layer=i;
		gl_Position=current_vertices[6];
		tex_coord_out=vertices[6];
		EmitVertex();
		gl_Layer=i;
		gl_Position=current_vertices[4];
		tex_coord_out=vertices[4];
		EmitVertex();
		EndPrimitive();
	
		//bottom
		gl_Layer=i;
		gl_Position=current_vertices[1];
		tex_coord_out=vertices[1];
		EmitVertex();
		gl_Layer=i;
		gl_Position=current_vertices[7];
		tex_coord_out=vertices[7];
		EmitVertex();
		gl_Layer=i;
		gl_Position=current_vertices[3];
		tex_coord_out=vertices[3];
		EmitVertex();
		gl_Layer=i;
		gl_Position=current_vertices[5];
		tex_coord_out=vertices[5];
		EmitVertex();
		EndPrimitive();
	}
	
}