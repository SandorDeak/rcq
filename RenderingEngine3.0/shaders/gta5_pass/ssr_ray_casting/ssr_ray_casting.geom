#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0) uniform ssr_ray_casting_data
{
	mat4 proj_x_view;
	vec3 view_pos;
	float ray_length;
} data; 

layout(set=0, binding=1) uniform sampler2D normal_tex;
layout(set=0, binding=2) uniform sampler2D pos_tex;

layout(invocations=1) in;

layout(points) in;

layout(location=0) in ivec2 fragment_id_in[1];

layout(location=0) out vec2 fragment_id_out;

layout(line_strip, max_vertices=2) out;

void main()
{
	vec3 pos=texture(pos_tex, gl_in[0].gl_Position.xy).xyz;
	vec3 n=texture(normal_tex, gl_in[0].gl_Position.xy).xyz;
	vec3 v=normalize(pos-data.view_pos);
	vec3 r=reflect(v, n);
	
	fragment_id_out=gl_in[0].gl_Position.xy;
	gl_Position=data.proj_x_view*vec4(pos, 1.f);
	EmitVertex();
	
	fragment_id_out=gl_in[0].gl_Position.xy;
	gl_Position=data.proj_x_view*vec4(pos+/*data.ray_length*/10.f*r, 1.f); 
	EmitVertex();

	EndPrimitive();
}