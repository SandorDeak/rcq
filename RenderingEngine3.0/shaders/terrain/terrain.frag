#version 450
#extension GL_ARB_separate_shader_objects : enable

uint MAX_MATERIAL_COUNT=4;

//material flags
const uint COLOR_TEX_FLAG_BIT=1;
const uint ROUGHNESS_TEX_FLAG_BIT=2;
const uint METAL_TEX_FLAG_BIT=4;
const uint NORMAL_TEX_FLAG_BIT=8;
const uint HEIGHT_TEX_FLAG_BIT=16;
const uint AO_TEX_FLAG_BIT=32;

layout (set=2, binding=0) uniform material_data0
{
	vec3 color;
	uint padding0;
	float roughness;
	float metal;
	float height_scale;
	uint tex_flags;
} m0;

layout (set=2, binding=1) uniform sampler2D color_tex0;
layout (set=2, binding=2) uniform sampler2D roughness_tex0;
layout (set=2, binding=3) uniform sampler2D metal_tex0;
layout (set=2, binding=4) uniform sampler2D normal_tex0;
layout (set=2, binding=5) uniform sampler2D height_tex0;
layout (set=2, binding=6) uniform sampler2D ao_tex0;

layout (set=3, binding=0) uniform material_data1
{
	vec3 color;
	uint padding0;
	float roughness;
	float metal;
	float height_scale;
	uint tex_flags;
} m1;

layout (set=3, binding=1) uniform sampler2D color_tex1;
layout (set=3, binding=2) uniform sampler2D roughness_tex1;
layout (set=3, binding=3) uniform sampler2D metal_tex1;
layout (set=3, binding=4) uniform sampler2D normal_tex1;
layout (set=3, binding=5) uniform sampler2D height_tex1;
layout (set=3, binding=6) uniform sampler2D ao_tex1;

layout (set=4, binding=0) uniform material_data2
{
	vec3 color;
	uint padding0;
	float roughness;
	float metal;
	float height_scale;
	uint tex_flags;
} m2;

layout (set=4, binding=1) uniform sampler2D color_tex2;
layout (set=4, binding=2) uniform sampler2D roughness_tex2;
layout (set=4, binding=3) uniform sampler2D metal_tex2;
layout (set=4, binding=4) uniform sampler2D normal_tex2;
layout (set=4, binding=5) uniform sampler2D height_tex2;
layout (set=4, binding=6) uniform sampler2D ao_tex2;

layout (set=5, binding=0) uniform material_data3
{
	vec3 color;
	uint padding0;
	float roughness;
	float metal;
	float height_scale;
	uint tex_flags;
} m3;

layout (set=5, binding=1) uniform sampler2D color_tex3;
layout (set=5, binding=2) uniform sampler2D roughness_tex3;
layout (set=5, binding=3) uniform sampler2D metal_tex3;
layout (set=5, binding=4) uniform sampler2D normal_tex3;
layout (set=5, binding=5) uniform sampler2D height_tex3;
layout (set=5, binding=6) uniform sampler2D ao_tex3;


layout(location=0) in vec4 tex_mask_in;
layout(location=1) in vec2 tex_coord_in;
layout(location=2) in vec3 pos_in;
layout(location=3) in vec3 view_in;
layout(location=4) in mat3 TBN_in;

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
	mat3 TBN=gram_schmidt(TBN_in);
	
	vec3 color=vec3(0.f);
	float roughness=0.f;
	float metal=0.f;
	vec3 n=vec3(0.f);
	float ao_factor=0.f;
		
	if (tex_mask_in.x!=0.f)
	{		
		vec2 tex_coord;
		if ((m0.tex_flags & HEIGHT_TEX_FLAG_BIT)==HEIGHT_TEX_FLAG_BIT)
		{
			//parallax occlusion
			vec3 view_tangent=-transpose(TBN)*normalize(view_in);
			if (view_tangent.z>-0.0000001f)
			{
				tex_coord=tex_coord_in;
			}
			else
			{		
				view_tangent*=(-m0.height_scale*max(0.2f, view_tangent.z)/view_tangent.z);
		
				float current_height=m0.height_scale*texture(height_tex0, tex_coord_in).x;
				float old_height=current_height;
				vec3 displacement=vec3(0.f, 0.f, m0.height_scale);	
				tex_coord=tex_coord_in;

				while (displacement.z>current_height)
				{
					displacement+=view_tangent;
					old_height=current_height;
					tex_coord=tex_coord_in+displacement.xy;
					current_height=m0.height_scale*texture(height_tex0, tex_coord).x;
				}
		
				tex_coord=tex_coord_in+displacement.xy+(view_tangent.xy)*(displacement.z-current_height)/(view_tangent.z-current_height-old_height);
			}
		}
		else
		{
			tex_coord=tex_coord_in;
		}
		if((m0.tex_flags & COLOR_TEX_FLAG_BIT)==COLOR_TEX_FLAG_BIT)
		{
			color+=tex_mask_in.x*pow(texture(color_tex0, tex_coord).xyz, vec3(2.2f));
		}
		else
		{
			color+=tex_mask_in.x*pow(m0.color, vec3(2.2f));
		}
		if((m0.tex_flags & ROUGHNESS_TEX_FLAG_BIT)==ROUGHNESS_TEX_FLAG_BIT)
		{
			roughness+=tex_mask_in.x*texture(roughness_tex0, tex_coord).x;
		}
		else
		{
			roughness+=tex_mask_in.x*m0.roughness;
		}
		if((m0.tex_flags & METAL_TEX_FLAG_BIT)==METAL_TEX_FLAG_BIT)
		{
			metal+=tex_mask_in.x*texture(metal_tex0, tex_coord).x;
		}
		else
		{
			metal+=tex_mask_in.x*m0.metal;
		}
		if ((m0.tex_flags & NORMAL_TEX_FLAG_BIT)==NORMAL_TEX_FLAG_BIT)
		{
			vec3 normal_raw=texture(normal_tex0, tex_coord).xyz;
			normal_raw=normal_raw*2.0f-1.0f;
			n+=tex_mask_in.x*TBN*normal_raw;
		}
		else
		{
			n+=tex_mask_in.x*TBN[2];
		}
		if ((m0.tex_flags & AO_TEX_FLAG_BIT)==AO_TEX_FLAG_BIT)
		{
			ao_factor+=tex_mask_in.x*texture(ao_tex0, tex_coord).x;
		}
		else
		{
			ao_factor+=tex_mask_in.x;
		}
	}
	
	

	
	if (tex_mask_in.y!=0.f)
	{		
		vec2 tex_coord;
		if ((m1.tex_flags & HEIGHT_TEX_FLAG_BIT)==HEIGHT_TEX_FLAG_BIT)
		{
			//parallax occlusion
			vec3 view_tangent=-transpose(TBN)*normalize(view_in);
			if (view_tangent.z>-0.0000001f)
			{
				tex_coord=tex_coord_in;
			}
			else
			{		
				view_tangent*=(-m1.height_scale*max(0.2f, view_tangent.z)/view_tangent.z);
		
				float current_height=m1.height_scale*texture(height_tex1, tex_coord_in).x;
				float old_height=current_height;
				vec3 displacement=vec3(0.f, 0.f, m1.height_scale);	
				tex_coord=tex_coord_in;

				while (displacement.z>current_height)
				{
					displacement+=view_tangent;
					old_height=current_height;
					tex_coord=tex_coord_in+displacement.xy;
					current_height=m1.height_scale*texture(height_tex1, tex_coord).x;
				}
		
				tex_coord=tex_coord_in+displacement.xy+(view_tangent.xy)*(displacement.z-current_height)/(view_tangent.z-current_height-old_height);
			}
		}
		else
		{
			tex_coord=tex_coord_in;
		}
		if((m1.tex_flags & COLOR_TEX_FLAG_BIT)==COLOR_TEX_FLAG_BIT)
		{
			color+=tex_mask_in.y*pow(texture(color_tex1, tex_coord).xyz, vec3(2.2f));
		}
		else
		{
			color+=tex_mask_in.y*pow(m1.color, vec3(2.2f));
		}
		if((m1.tex_flags & ROUGHNESS_TEX_FLAG_BIT)==ROUGHNESS_TEX_FLAG_BIT)
		{
			roughness+=tex_mask_in.y*texture(roughness_tex1, tex_coord).x;
		}
		else
		{
			roughness+=tex_mask_in.y*m1.roughness;
		}
		if((m1.tex_flags & METAL_TEX_FLAG_BIT)==METAL_TEX_FLAG_BIT)
		{
			metal+=tex_mask_in.y*texture(metal_tex1, tex_coord).x;
		}
		else
		{
			metal+=tex_mask_in.y*m1.metal;
		}
		if ((m1.tex_flags & NORMAL_TEX_FLAG_BIT)==NORMAL_TEX_FLAG_BIT)
		{
			vec3 normal_raw=texture(normal_tex1, tex_coord).xyz;
			normal_raw=normal_raw*2.0f-1.0f;
			n+=tex_mask_in.y*TBN*normal_raw;
		}
		else
		{
			n+=tex_mask_in.y*TBN[2];
		}
		if ((m1.tex_flags & AO_TEX_FLAG_BIT)==AO_TEX_FLAG_BIT)
		{
			ao_factor+=tex_mask_in.y*texture(ao_tex1, tex_coord).x;
		}
		else
		{
			ao_factor+=tex_mask_in.y;
		}
		
		
		
	}
	
	
	if (tex_mask_in.z!=0.f)
	{		
		vec2 tex_coord;
		if ((m2.tex_flags & HEIGHT_TEX_FLAG_BIT)==HEIGHT_TEX_FLAG_BIT)
		{
			//parallax occlusion
			vec3 view_tangent=-transpose(TBN)*normalize(view_in);
			if (view_tangent.z>-0.0000001f)
			{
				tex_coord=tex_coord_in;
			}
			else
			{		
				view_tangent*=(-m2.height_scale*max(0.2f, view_tangent.z)/view_tangent.z);
		
				float current_height=m2.height_scale*texture(height_tex2, tex_coord_in).x;
				float old_height=current_height;
				vec3 displacement=vec3(0.f, 0.f, m2.height_scale);	
				tex_coord=tex_coord_in;

				while (displacement.z>current_height)
				{
					displacement+=view_tangent;
					old_height=current_height;
					tex_coord=tex_coord_in+displacement.xy;
					current_height=m2.height_scale*texture(height_tex2, tex_coord).x;
				}
		
				tex_coord=tex_coord_in+displacement.xy+(view_tangent.xy)*(displacement.z-current_height)/(view_tangent.z-current_height-old_height);
			}
		}
		else
		{
			tex_coord=tex_coord_in;
		}
		if((m2.tex_flags & COLOR_TEX_FLAG_BIT)==COLOR_TEX_FLAG_BIT)
		{
			color+=tex_mask_in.z*pow(texture(color_tex2, tex_coord).xyz, vec3(2.2f));
		}
		else
		{
			color+=tex_mask_in.z*pow(m2.color, vec3(2.2f));
		}
		if((m2.tex_flags & ROUGHNESS_TEX_FLAG_BIT)==ROUGHNESS_TEX_FLAG_BIT)
		{
			roughness+=tex_mask_in.z*texture(roughness_tex2, tex_coord).x;
		}
		else
		{
			roughness+=tex_mask_in.z*m2.roughness;
		}
		if((m2.tex_flags & METAL_TEX_FLAG_BIT)==METAL_TEX_FLAG_BIT)
		{
			metal+=tex_mask_in.z*texture(metal_tex2, tex_coord).x;
		}
		else
		{
			metal+=tex_mask_in.z*m2.metal;
		}
		if ((m2.tex_flags & NORMAL_TEX_FLAG_BIT)==NORMAL_TEX_FLAG_BIT)
		{
			vec3 normal_raw=texture(normal_tex2, tex_coord).xyz;
			normal_raw=normal_raw*2.0f-1.0f;
			n+=tex_mask_in.z*TBN*normal_raw;
		}
		else
		{
			n+=tex_mask_in.z*TBN[2];
		}
		if ((m2.tex_flags & AO_TEX_FLAG_BIT)==AO_TEX_FLAG_BIT)
		{
			ao_factor+=tex_mask_in.z*texture(ao_tex2, tex_coord).x;
		}
		else
		{
			ao_factor+=tex_mask_in.z;
		}	
	}
	
	
	if (tex_mask_in.w!=0.f)
	{		
		vec2 tex_coord;
		if ((m3.tex_flags & HEIGHT_TEX_FLAG_BIT)==HEIGHT_TEX_FLAG_BIT)
		{
			//parallax occlusion
			vec3 view_tangent=-transpose(TBN)*normalize(view_in);
			if (view_tangent.z>-0.0000001f)
			{
				tex_coord=tex_coord_in;
			}
			else
			{		
				view_tangent*=(-m3.height_scale*max(0.2f, view_tangent.z)/view_tangent.z);
		
				float current_height=m3.height_scale*texture(height_tex3, tex_coord_in).x;
				float old_height=current_height;
				vec3 displacement=vec3(0.f, 0.f, m3.height_scale);	
				tex_coord=tex_coord_in;

				while (displacement.z>current_height)
				{
					displacement+=view_tangent;
					old_height=current_height;
					tex_coord=tex_coord_in+displacement.xy;
					current_height=m3.height_scale*texture(height_tex3, tex_coord).x;
				}
		
				tex_coord=tex_coord_in+displacement.xy+(view_tangent.xy)*(displacement.z-current_height)/(view_tangent.z-current_height-old_height);
			}
		}
		else
		{
			tex_coord=tex_coord_in;
		}
		if((m3.tex_flags & COLOR_TEX_FLAG_BIT)==COLOR_TEX_FLAG_BIT)
		{
			color+=tex_mask_in.w*pow(texture(color_tex3, tex_coord).xyz, vec3(2.2f));
		}
		else
		{
			color+=tex_mask_in.w*pow(m3.color, vec3(2.2f));
		}
		if((m3.tex_flags & ROUGHNESS_TEX_FLAG_BIT)==ROUGHNESS_TEX_FLAG_BIT)
		{
			roughness+=tex_mask_in.w*texture(roughness_tex3, tex_coord).x;
		}
		else
		{
			roughness+=tex_mask_in.w*m3.roughness;
		}
		if((m3.tex_flags & METAL_TEX_FLAG_BIT)==METAL_TEX_FLAG_BIT)
		{
			metal+=tex_mask_in.w*texture(metal_tex3, tex_coord).x;
		}
		else
		{
			metal+=tex_mask_in.w*m3.metal;
		}
		if ((m3.tex_flags & NORMAL_TEX_FLAG_BIT)==NORMAL_TEX_FLAG_BIT)
		{
			vec3 normal_raw=texture(normal_tex3, tex_coord).xyz;
			normal_raw=normal_raw*2.0f-1.0f;
			n+=tex_mask_in.w*TBN*normal_raw;
		}
		else
		{
			n+=tex_mask_in.w*TBN[2];
		}
		if ((m3.tex_flags & AO_TEX_FLAG_BIT)==AO_TEX_FLAG_BIT)
		{
			ao_factor+=tex_mask_in.w*texture(ao_tex3, tex_coord).x;
		}
		else
		{
			ao_factor+=tex_mask_in.w;
		}	
	}
	
	pos_roughness_out=vec4(pos_in, roughness);
	basecolor_ssao_out=vec4(color, 1.f);
	metalness_ssds_out=vec4(metal, 1.f, 1.f, 1.f);
	normal_ao_out=vec4(normalize(n), ao_factor);
	
}