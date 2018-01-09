#version 450
#extension GL_ARB_separate_shader_objects : enable


//material flags
const uint COLOR_TEX_FLAG_BIT=1;
const uint ROUGHNESS_TEX_FLAG_BIT=2;
const uint METAL_TEX_FLAG_BIT=4;
const uint NORMAL_TEX_FLAG_BIT=8;
const uint HEIGHT_TEX_FLAG_BIT=16;
const uint AO_TEX_FLAG_BIT=32;

//const float HEIGHT_MAX_SAMPLE_COUNT=8.f;


layout (set=2, binding=0) uniform material_data
{
	vec3 color;
	uint padding0;
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
layout(location=4) in vec3 pos_in;
layout(location=5) in vec3 view_in;

layout(location=0) out vec4 pos_roughness_out;
layout(location=1) out vec4 basecolor_ssao_out;
layout(location=2) out vec4 metalness_ssds_out;
layout(location=3) out vec4 normal_ao_out;

mat3 gram_schmidt(mat3 m)
{
	mat3 ret;
	ret[2]=normalize(m[2]);
	ret[1]=normalize(m[1]-dot(m[1],ret[2])*ret[2]);
	ret[0]=normalize(m[0]-dot(m[0],ret[2])*ret[2]-dot(m[0],ret[1])*ret[1]);
	return ret;
}

void main()
{
	vec3 color;
	float roughness;
	float metal;
	vec3 n;
	float ao_factor;
	vec2 tex_coord;
	
	mat3 TBN=gram_schmidt(TBN_in);
	
	if ((mat.tex_flags & HEIGHT_TEX_FLAG_BIT)==HEIGHT_TEX_FLAG_BIT)
	{
		//parallax occlusion
		vec3 view_tangent=-transpose(TBN)*normalize(view_in);
		if (view_tangent.z>-0.0000001f)
		{
			discard;
		}
		
		//view_tangent/=(-mat.height_scale_scale*HEIGHT_MAX_SAMPLE_COUNT*view_tangent.z);
		
		view_tangent*=(-mat.height_scale*max(0.2f, view_tangent.z)/view_tangent.z);
		
		float current_height=mat.height_scale*texture(height_tex, tex_coord_in).x;
		float old_height=current_height;
		vec3 displacement=vec3(0.f, 0.f, mat.height_scale);	
		tex_coord=tex_coord_in;

		while (displacement.z>current_height)
		{
			displacement+=view_tangent;
			old_height=current_height;
			tex_coord=tex_coord_in+displacement.xy;
			current_height=mat.height_scale*texture(height_tex, tex_coord).x;
		}
		
		tex_coord=tex_coord_in+displacement.xy+(view_tangent.xy)*(displacement.z-current_height)/(view_tangent.z-current_height-old_height);
	}
	else
	{
		tex_coord=tex_coord_in;
	}
	if((mat.tex_flags & COLOR_TEX_FLAG_BIT)==COLOR_TEX_FLAG_BIT)
	{
		color=pow(texture(color_tex, tex_coord).xyz, vec3(2.2f));
	}
	else
	{
		color=pow(mat.color, vec3(2.2f));
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
		n=normalize(TBN*normal_raw);
	}
	else
	{
		n=TBN[2];
	}
	if ((mat.tex_flags & AO_TEX_FLAG_BIT)==AO_TEX_FLAG_BIT)
	{
		ao_factor=texture(ao_tex, tex_coord).x;
	}
	else
	{
		ao_factor=1.f;
	}
	
	
	
	pos_roughness_out=vec4(pos_in, roughness);
	basecolor_ssao_out=vec4(color, 1.f);
	metalness_ssds_out=vec4(metal, 1.f, 1.f, 1.f);
	normal_ao_out=vec4(n, ao_factor);
}