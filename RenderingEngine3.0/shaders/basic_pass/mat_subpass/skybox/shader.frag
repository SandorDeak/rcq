#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=1, binding=0) uniform samplerCube skybox_tex;

layout(location=0) in vec3 tex_coord;

layout(location=0) out vec4 color_out;

void main()
{
	color_out=vec4(texture(skybox_tex, tex_coord).xyz, 1.f);
}