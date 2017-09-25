#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint POSITION=0;
const uint 

layout(set=0, binding=0) uniform omni_light_data
{
	
	
} old;

layout(set=0, binding=1) uniform samplerCube shadow_map;

//G-buffer
layout(input_attachment_index=0) in vec4 pos;
layout(input_attachment_index=1) in vec4 F0_roughness;
layout(input_attachment_index=2) in vec4 albedo_ao;
layout(input_attachment_index=3) in vec4 view_dir;
layout(input_attachment_index=4) in vec4 normal;

//output
layout(location=0) out vec4 color_out;