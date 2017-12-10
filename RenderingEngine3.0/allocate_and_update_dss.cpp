#include "gta5_pass.h"

#include "enum_gp.h"
#include "enum_cp.h"
#include "res_data.h"

using namespace rcq;

void gta5_pass::allocate_and_update_dss()
{
	///////////////////////////////////////////////
	//allocate


	//allocate graphics dss
	{
		VkDescriptorSet dss[GP_COUNT];
		VkDescriptorSetLayout dsls[GP_COUNT];

		VkDescriptorSetAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = m_dp;
		alloc.descriptorSetCount = GP_COUNT;
		alloc.pSetLayouts = dsls;

		for (uint32_t i = 0; i < GP_COUNT; ++i)
			dsls[i] = m_gps[i].dsl;

		assert(vkAllocateDescriptorSets(m_base.device, &alloc, dss) == VK_SUCCESS);

		for (uint32_t i = 0; i < GP_COUNT; ++i)
			m_gps[i].ds = dss[i];
	}
	//allocate compute dss
	{
		VkDescriptorSet dss[CP_COUNT];
		VkDescriptorSetLayout dsls[CP_COUNT];

		VkDescriptorSetAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = m_dp;
		alloc.descriptorSetCount = CP_COUNT;
		alloc.pSetLayouts = dsls;

		for (uint32_t i = 0; i < CP_COUNT; ++i)
			dsls[i] = m_cps[i].dsl;

		assert(vkAllocateDescriptorSets(m_base.device, &alloc, dss) == VK_SUCCESS);

		for (uint32_t i = 0; i < CP_COUNT; ++i)
			m_cps[i].ds = dss[i];
	}

	///////////////////////////////////////////////
	//update
	constexpr uint32_t WRITE_SIZE = 16;
	VkWriteDescriptorSet w[WRITE_SIZE] = {};
	for (uint32_t i = 0; i < WRITE_SIZE; ++i)
	{
		w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[i].dstBinding = i;
		w[i].descriptorCount = 1;
		w[i].dstArrayElement = 0;
	}

	//environment map gen mat
	{
		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_ENVIRONMENT_MAP_GEN_MAT];
		ub.range = sizeof(resource_data<RES_DATA_ENVIRONMENT_MAP_GEN_MAT>);

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].ds;
		w[0].pBufferInfo = &ub;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}

	//environment map gen skybox
	{
		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX];
		ub.range = sizeof(resource_data<RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX>);

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].ds;
		w[0].pBufferInfo = &ub;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}

	//gbuffer gen
	{
		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_GBUFFER_GEN];
		ub.range = sizeof(resource_data<RES_DATA_GBUFFER_GEN>);

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_OPAQUE_OBJ_DRAWER].ds;
		w[0].pBufferInfo = &ub;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}

	//dir shadow map gen
	{

		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_DIR_SHADOW_MAP_GEN];
		ub.range = sizeof(resource_data<RES_DATA_DIR_SHADOW_MAP_GEN>);

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_DIR_SHADOW_MAP_GEN].ds;
		w[0].pBufferInfo = &ub;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}

	//ss dir shadow map gen
	{
		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_SS_DIR_SHADOW_MAP_GEN];
		ub.range = sizeof(resource_data<RES_DATA_SS_DIR_SHADOW_MAP_GEN>);

		VkDescriptorImageInfo shadow_tex = {};
		shadow_tex.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadow_tex.imageView = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].view;
		shadow_tex.sampler = m_samplers[SAMPLER_GENERAL];

		VkDescriptorImageInfo pos = {};
		pos.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;

		VkDescriptorImageInfo normal = {};
		normal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normal.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_GEN].ds;
		w[0].pBufferInfo = &ub;

		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_GEN].ds;
		w[1].pImageInfo = &shadow_tex;

		w[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[2].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_GEN].ds;
		w[2].pImageInfo = &pos;

		w[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[3].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_GEN].ds;
		w[3].pImageInfo = &normal;

		vkUpdateDescriptorSets(m_base.device, 4, w, 0, nullptr);
	}

	//ss dir shadow map blur
	{
		VkDescriptorImageInfo pos;
		pos.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		pos.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo shadow;
		shadow.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadow.imageView = m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].view;
		shadow.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo normal;
		normal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normal.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		normal.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[0].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].ds;
		w[0].pImageInfo = &pos;

		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].ds;
		w[1].pImageInfo = &shadow;

		w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[2].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].ds;
		w[2].pImageInfo = &normal;

		vkUpdateDescriptorSets(m_base.device, 3, w, 0, nullptr);
	}

	//ssao gen
	{
		VkDescriptorImageInfo pos;
		pos.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		pos.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo normal;
		normal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normal.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		normal.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[0].dstSet = m_gps[GP_SSAO_GEN].ds;
		w[0].pImageInfo = &pos;

		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstSet = m_gps[GP_SSAO_GEN].ds;
		w[1].pImageInfo = &normal;

		vkUpdateDescriptorSets(m_base.device, 2, w, 0, nullptr);
	}

	//ssao blur
	{
		VkDescriptorImageInfo ssao;
		ssao.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		ssao.imageView = m_res_image[RES_IMAGE_SSAO_MAP].view;
		ssao.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[0].dstSet = m_gps[GP_SSAO_BLUR].ds;
		w[0].pImageInfo = &ssao;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}

	//ssr ray casting
	{
		VkDescriptorBufferInfo data;
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_SSR_RAY_CASTING];
		data.range = sizeof(resource_data<RES_DATA_SSR_RAY_CASTING>);

		VkDescriptorImageInfo normal_tex;
		normal_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normal_tex.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		normal_tex.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo pos_tex;
		pos_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos_tex.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		pos_tex.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo ray_end_fragment_ids;
		ray_end_fragment_ids.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		ray_end_fragment_ids.imageView = m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].view;

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_SSR_RAY_CASTING].ds;
		w[0].pBufferInfo = &data;

		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstSet = m_gps[GP_SSR_RAY_CASTING].ds;
		w[1].pImageInfo = &normal_tex;

		w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[2].dstSet = m_gps[GP_SSR_RAY_CASTING].ds;
		w[2].pImageInfo = &pos_tex;

		w[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		w[3].dstSet = m_gps[GP_SSR_RAY_CASTING].ds;
		w[3].pImageInfo = &ray_end_fragment_ids;

		vkUpdateDescriptorSets(m_base.device, 4, w, 0, nullptr);
	}

	//refraction map gen
	{
		VkDescriptorBufferInfo data;
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_REFRACTION_MAP_GEN];
		data.range = sizeof(resource_data<RES_DATA_REFRACTION_MAP_GEN>);

		VkDescriptorImageInfo preimage;
		preimage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		preimage.imageView = m_res_image[RES_IMAGE_PREIMAGE].view;
		preimage.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_REFRACTION_IMAGE_GEN].ds;
		w[0].pBufferInfo = &data;

		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstSet = m_gps[GP_REFRACTION_IMAGE_GEN].ds;
		w[1].pImageInfo = &preimage;

		vkUpdateDescriptorSets(m_base.device, 2, w, 0, nullptr);
	}

	//image assembler
	{
		VkDescriptorImageInfo gb_pos_roughness;
		gb_pos_roughness.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_pos_roughness.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		gb_pos_roughness.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo gb_BASECOLOR_SSAO;
		gb_BASECOLOR_SSAO.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_BASECOLOR_SSAO.imageView = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].view;

		VkDescriptorImageInfo gb_METALNESS_SSDS;
		gb_METALNESS_SSDS.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_METALNESS_SSDS.imageView = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].view;

		VkDescriptorImageInfo gb_normal_ao;
		gb_normal_ao.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_normal_ao.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		gb_normal_ao.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo em;
		em.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		em.imageView = m_res_image[RES_IMAGE_ENVIRONMENT_MAP].view;
		//em.imageView = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].view;
		em.sampler = m_samplers[SAMPLER_GENERAL];

		VkDescriptorBufferInfo ub;
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_IMAGE_ASSEMBLER];
		ub.range = sizeof(resource_data<RES_DATA_IMAGE_ASSEMBLER>);

		VkDescriptorImageInfo prev_image;
		prev_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		prev_image.imageView = m_res_image[RES_IMAGE_PREV_IMAGE].view;
		prev_image.sampler = m_samplers[SAMPLER_GENERAL];

		VkDescriptorImageInfo ssr_ray_casting_coords;
		ssr_ray_casting_coords.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		ssr_ray_casting_coords.imageView = m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].view;

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[0].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[0].pImageInfo = &gb_BASECOLOR_SSAO;

		w[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[1].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[1].pImageInfo = &gb_METALNESS_SSDS;

		w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[2].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[2].pImageInfo = &gb_pos_roughness;

		w[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[3].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[3].pImageInfo = &gb_normal_ao;

		w[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[4].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[4].pImageInfo = &em;

		w[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[5].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[5].pBufferInfo = &ub;

		w[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[6].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[6].pImageInfo = &prev_image;

		w[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		w[7].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[7].pImageInfo = &ssr_ray_casting_coords;

		vkUpdateDescriptorSets(m_base.device, 8, w, 0, nullptr);
	}

	//sky drawer
	{
		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_SKY_DRAWER];
		data.range = sizeof(resource_data<RES_DATA_SKY_DRAWER>);

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_SKY_DRAWER].ds;
		w[0].pBufferInfo = &data;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}

	//sun drawer
	{
		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_SUN_DRAWER];
		data.range = sizeof(resource_data<RES_DATA_SUN_DRAWER>);

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_SUN_DRAWER].ds;
		w[0].pBufferInfo = &data;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}

	//terrain drawer
	{
		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_TERRAIN_DRAWER];
		data.range = sizeof(resource_data<RES_DATA_TERRAIN_DRAWER>);

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_TERRAIN_DRAWER].ds;
		w[0].pBufferInfo = &data;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}

	//terrain tile request
	{
		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_TERRAIN_TILE_REQUEST];
		data.range = sizeof(resource_data<RES_DATA_TERRAIN_TILE_REQUEST>);

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_cps[CP_TERRAIN_TILE_REQUEST].ds;
		w[0].pBufferInfo = &data;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}

	//water drawer
	{

		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_WATER_DRAWER];
		data.range = sizeof(resource_data<RES_DATA_WATER_DRAWER>);

		VkDescriptorImageInfo refr_tex;
		refr_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		refr_tex.imageView = m_res_image[RES_IMAGE_REFRACTION_IMAGE].view;
		refr_tex.sampler = m_samplers[SAMPLER_GENERAL];

		VkDescriptorImageInfo pos_tex;
		pos_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos_tex.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		pos_tex.sampler = m_samplers[SAMPLER_GENERAL];

		VkDescriptorImageInfo normal_tex;
		normal_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normal_tex.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		normal_tex.sampler = m_samplers[SAMPLER_GENERAL];

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_gps[GP_WATER_DRAWER].ds;
		w[0].pBufferInfo = &data;

		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstSet = m_gps[GP_WATER_DRAWER].ds;
		w[1].pImageInfo = &refr_tex;

		w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[2].dstSet = m_gps[GP_WATER_DRAWER].ds;
		w[2].pImageInfo = &pos_tex;

		w[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[3].dstSet = m_gps[GP_WATER_DRAWER].ds;
		w[3].pImageInfo = &normal_tex;

		vkUpdateDescriptorSets(m_base.device, 4, w, 0, nullptr);
	}

	//water compute
	{

		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_WATER_COMPUTE];
		data.range = sizeof(resource_data<RES_DATA_WATER_COMPUTE>);

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstSet = m_cps[CP_WATER_FFT].ds;
		w[0].pBufferInfo = &data;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}
	//bloom blur
	{
		VkDescriptorImageInfo blur_im;
		blur_im.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		blur_im.imageView = m_res_image[RES_IMAGE_BLOOM_BLUR].view;

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		w[0].dstSet = m_cps[CP_BLOOM].ds;
		w[0].pImageInfo = &blur_im;

		vkUpdateDescriptorSets(m_base.device, 1, w, 0, nullptr);
	}

	//postprocessing
	{
		VkDescriptorImageInfo preimage = {};
		preimage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		preimage.imageView = m_res_image[RES_IMAGE_PREIMAGE].view;
		preimage.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo bloom_blur;
		bloom_blur.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		bloom_blur.imageView = m_res_image[RES_IMAGE_BLOOM_BLUR].view;
		bloom_blur.sampler = m_samplers[SAMPLER_GENERAL];

		w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[0].dstSet = m_gps[GP_POSTPROCESSING].ds;
		w[0].pImageInfo = &preimage;

		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstSet = m_gps[GP_POSTPROCESSING].ds;
		w[1].pImageInfo = &bloom_blur;

		vkUpdateDescriptorSets(m_base.device, 2, w, 0, nullptr);
	}
}