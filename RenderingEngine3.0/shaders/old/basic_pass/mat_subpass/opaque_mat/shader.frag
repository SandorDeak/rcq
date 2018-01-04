#version 450
#extension GL_ARB_separate_shader_objects : enable

#define PI 3.1415926535897f

//material flags
const uint COLOR_TEX_FLAG_BIT=1;
const uint ROUGHNESS_TEX_FLAG_BIT=2;
const uint METAL_TEX_FLAG_BIT=4;
const uint NORMAL_TEX_FLAG_BIT=8;
const uint HEIGHT_TEX_FLAG_BIT=16;
const uint AO_TEX_FLAG_BIT=32;

layout(set=0, binding=0) uniform camera_data
{
	mat4 proj_x_view;
	vec3 pos;
} cam;

layout (set=2, binding=0) uniform material_data
{
	vec3 color;
	float roughness;
	float metal;
	float height_scale;
	uint tex_flags;
} mat;

layout (set=2, binding=1) uniform sampler2D color_tex;
layout (set=2, binding=2) uniform sampler2D roughness_tex;
layout (set=2, binding=3) uniform sampler2D metal_tex;
layout (set=2, binding=4) uniform sampler2D normal_tex;
layout (set=2, binding=5) uniform sampler2D height_tex;
layout (set=2, binding=6) uniform sampler2D ao_tex;

layout(location=0) in vec2 tex_coord_in;
layout(location=1) in mat3 TBN_in;
layout(location=4) in vec3 pos_world_in;

layout(location=0) out vec4 pos_out;
layout(location=1) out vec4 F0_roughness_out;
layout(location=2) out vec4 albedo_ao_out;
layout(location=3) out vec4 normal_out;
layout(location=4) out vec4 view_dir_out;

void main()
{
	vec3 color;
	float roughness;
	float metal;
	vec3 n;
	float ao_factor;
	vec2 tex_coord;
	
	
	if ((mat.tex_flags & HEIGHT_TEX_FLAG_BIT)==HEIGHT_TEX_FLAG_BIT)
	{
		//tex_coord displacement
		vec3 view_in_tangent_space=transpose(TBN_in)*(pos_world_in-cam.pos);
		if (view_in_tangent_space.z>=0.f)
		{
			discard;
		}
		
		view_in_tangent_space*=(-mat.height_scale*max(0.1f, view_in_tangent_space.z)/view_in_tangent_space.z);
		
		float current_height=mat.height_scale*texture(height_tex, tex_coord_in).x;
		float old_height=current_height;
		vec3 displacement=vec3(0.f, 0.f, mat.height_scale);	
		tex_coord=tex_coord_in;

		while (displacement.z>current_height)
		{
			displacement+=view_in_tangent_space;
			old_height=current_height;
			tex_coord=tex_coord_in+displacement.xy;
			current_height=mat.height_scale*texture(height_tex, tex_coord).x;
		}
		
		tex_coord=tex_coord_in+displacement.xy+(view_in_tangent_space.xy)*(displacement.z-current_height)/(view_in_tangent_space.z-current_height-old_height);
		/*if (tex_coord.x>1.f || tex_coord.x<0.f || tex_coord.y> 1.f || tex_coord.y<0.f)
			{
				discard;
			}*/
	}
	else
	{
		tex_coord=tex_coord_in;
	}
	if((mat.tex_flags & COLOR_TEX_FLAG_BIT)==COLOR_TEX_FLAG_BIT)
	{
		color=pow(texture(color_tex, tex_coord).xyz, vec3(2.2));
	}
	else
	{
		color=mat.color;
	}
	if((mat.tex_flags & ROUGHNESS_TEX_FLAG_BIT)==ROUGHNESS_TEX_FLAG_BIT)
	{
		roughness=texture(roughness_tex, tex_coord).x;
	}
	else
	{
		roughness=mat.roughness;
	}
	if((mat.tex_flags & METAL_TEX_FLAG_BIT)==METAL_TEX_FLAG_BIT)
	{
		metal=texture(metal_tex, tex_coord).x;
	}
	else
	{
		metal=mat.metal;
	}
	if ((mat.tex_flags & NORMAL_TEX_FLAG_BIT)==NORMAL_TEX_FLAG_BIT)
	{
		vec3 normal_raw=texture(normal_tex, tex_coord).xyz;
		normal_raw=normal_raw*2.0f-1.0f;
		n=normalize(TBN_in*normal_raw);
	}
	else
	{
		n=normalize(TBN_in[2]);
	}
	if ((mat.tex_flags & AO_TEX_FLAG_BIT)==AO_TEX_FLAG_BIT)
	{
		ao_factor=texture(ao_tex, tex_coord).x;
	}
	else
	{
		ao_factor=1.f;
	}
	
	vec3 F0=mix(vec3(0.04f), color, metal);
	vec3 albedo=mix(color, vec3(0.0f) , metal);
	vec3 v=normalize(cam.pos-pos_world_in);
	
	pos_out=vec4(pos_world_in, 1.f);
	F0_roughness_out=vec4(F0, roughness);
	albedo_ao_out=vec4(albedo, ao_factor);
	normal_out=vec4(n, 1.f);
	view_dir_out=vec4(v, 1.f);	
}