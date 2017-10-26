#version 450
#extension GL_ARB_separate_shader_objects : enable

#define PI 3.1415926535897f

const float r=32.f*PI/(180.f*60.f);
const float dtheta=2.f*PI/16.f;


layout(set=0, binding=0) uniform sun_data
{
	mat4 proj_x_view_at_origin;
	vec3 light_dir;
	uint padding0;
	vec3 helper_dir;
	float height;
	vec3 irradiance;
} data;

layout(location=0) out float distance_from_center_out;
layout(location=1) out vec3 view_dir_out;

void main()
{	
	if (gl_VertexIndex==0)
	{
		distance_from_center_out=0.f;
		view_dir_out=-data.light_dir;
		vec4 pos=data.proj_x_view_at_origin*vec4(-data.light_dir, 1.f);
		gl_Position=pos.xyww;
	}
	else
	{
		distance_from_center_out=PI/2.f;
		float angle=dtheta*(float(gl_VertexIndex)-1.f);
		vec3 edge=-data.light_dir+r*data.helper_dir*cos(angle)+cross(data.light_dir, data.helper_dir)*r*sin(angle);
		view_dir_out=normalize(edge);
		vec4 pos=data.proj_x_view_at_origin*vec4(edge, 1.f);
		gl_Position=pos.xyww;
	}
}

