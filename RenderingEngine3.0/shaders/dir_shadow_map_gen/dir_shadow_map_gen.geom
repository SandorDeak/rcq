#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint FRUSTUM_SPLIT_COUNT=4;

layout (set=0, binding=0) uniform dir_shadow_map_gen_data
{
	mat4 projs[FRUSTUM_SPLIT_COUNT];
} data;

layout(invocations=FRUSTUM_SPLIT_COUNT) in;

layout(triangles) in;

layout(triangle_strip, max_vertices=3) out;

void main()
{
	for (uint i=0; i<3; ++i)
	{
		gl_Layer=gl_InvocationID;
		gl_Position=data.projs[gl_InvocationID]*gl_in[i].gl_Position;
		EmitVertex();
	}
}


