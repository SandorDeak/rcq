#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint MAX_TILE_COUNT=128;//2048;
const uint MAX_TILE_COUNT_LOG2=11;

layout(quads, equal_spacing) in;

layout(set=0, binding=0) uniform terrain_data
{
	mat4 proj_x_view;
	vec3 view_pos;
	uint padding0;
} data;

layout(set=1, binding=0) uniform terrain_buffer
{
	vec2 terrain_size_in_meters;
	vec2 meter_per_tile_size_length;
	ivec2 tile_count;
	float mip_level_count;
	float height_scale;
} terr;

layout(set=1, binding=1) uniform sampler2D terrain_tex;

layout(location=0) patch in float mip_level_in[5];

layout(location=0) out vec4 tex_mask_out;
layout(location=1) out vec2 tex_coords_out;
layout(location=2) out vec3 pos_out;
layout(location=3) out vec3 view_out;
layout(location=4) out mat3 TBN_out;

void main()
{
	vec4 mid1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 mid2 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.x);
	vec4 pos = mix(mid1, mid2, gl_TessCoord.y);
	
	vec2 tex_coords=pos.xz/terr.terrain_size_in_meters;
	
	float mip_level=mip_level_in[4];
	if (gl_TessCoord.x<0.3f)
		mip_level=max(mip_level, mip_level_in[0]);
	if (gl_TessCoord.x>0.7f)
		mip_level=max(mip_level, mip_level_in[2]);
	if (gl_TessCoord.y<0.3f)
		mip_level=max(mip_level, mip_level_in[1]);
	if (gl_TessCoord.y>0.7f)
		mip_level=max(mip_level, mip_level_in[3]);	
	
	vec4 tex_val=textureLod(terrain_tex, tex_coords, mip_level);
	/*ivec2 tex_size=textureSize(terrain_tex, int(mip_level));
	vec4 tex_val=texelFetch(terrain_tex, ivec2(tex_coords*float(tex_size)), int(mip_level));*/
	float height=tex_val.z*terr.height_scale;
	vec2 grad=terr.height_scale*tex_val.xy/terr.terrain_size_in_meters;
	pos.y=height;
	
	uint mask=floatBitsToUint(tex_val.w);	
	tex_mask_out=vec4(
		float(mask & 255),
		float((mask>>8) & 255),
		float((mask>>16) & 255),
		float((mask>>24) & 255)
	)/255.f;
	
	tex_coords_out=pos.xz;
	
	vec3 tangent=normalize(vec3(1.f, grad.x, 0.f));
	vec3 normal=vec3(0.f, 1.f, 0.f);//normalize(vec3(-grad.x, 1.f, -grad.y));
	vec3 bitangent=cross(tangent, normal);
	
	TBN_out=mat3(tangent, bitangent, normal);
	
	pos_out=pos.xyz;
	view_out=normalize(pos.xyz-data.view_pos);
		
	gl_Position=data.proj_x_view*pos;
}


























