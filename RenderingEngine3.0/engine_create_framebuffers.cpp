#include "engine.h"

#include "rps.h"

#include "enum_res_image.h"

#include "const_environment_map_size.h"
#include "const_dir_shadow_map_size.h"
#include "const_swap_chain_image_extent.h"

using namespace rcq;

void engine::create_framebuffers()
{
	//environment map gen
	{
		using ATT = rp_create_info<RP_ENVIRONMENT_MAP_GEN>::ATT;

		VkImageView atts[ATT::ATT_COUNT];
		atts[ATT::ATT_COLOR] = m_res_image[RES_IMAGE_ENVIRONMENT_MAP].view;
		atts[ATT::ATT_DEPTH] = m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT::ATT_COUNT;
		fb.pAttachments = atts;
		fb.height = ENVIRONMENT_MAP_SIZE;
		fb.width = ENVIRONMENT_MAP_SIZE;
		fb.layers = 6;
		fb.renderPass = m_rps[RP_ENVIRONMENT_MAP_GEN];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs[FB_ENVIRONMENT_MAP_GEN]) == VK_SUCCESS);
	}

	//dir shadow map gen
	{
		using ATT = rp_create_info<RP_DIR_SHADOW_MAP_GEN>::ATT;

		VkImageView atts[ATT::ATT_COUNT];
		atts[ATT::ATT_DEPTH] = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT::ATT_COUNT;
		fb.pAttachments = atts;
		fb.height = DIR_SHADOW_MAP_SIZE;
		fb.width = DIR_SHADOW_MAP_SIZE;
		fb.layers = FRUSTUM_SPLIT_COUNT;
		fb.renderPass = m_rps[RP_DIR_SHADOW_MAP_GEN];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs[FB_DIR_SHADOW_MAP_GEN]) == VK_SUCCESS);
	}

	//gbuffer assembler
	{
		using ATT = rp_create_info<RP_GBUFFER_ASSEMBLER>::ATT;

		VkImageView atts[ATT::ATT_COUNT];
		atts[ATT::ATT_POS_ROUGHNESS] = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		atts[ATT::ATT_METALNESS_SSDS] = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].view;
		atts[ATT::ATT_BASECOLOR_SSAO] = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].view;
		atts[ATT::ATT_NORMAL_AO] = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		atts[ATT::ATT_DEPTHSTENCIL] = m_res_image[RES_IMAGE_GB_DEPTH].view;
		atts[ATT::ATT_SS_DIR_SHADOW_MAP] = m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT::ATT_COUNT;
		fb.pAttachments = atts;
		fb.height = SWAP_CHAIN_IMAGE_EXTENT.height;
		fb.width = SWAP_CHAIN_IMAGE_EXTENT.width;
		fb.layers = 1;
		fb.renderPass = m_rps[RP_GBUFFER_ASSEMBLER];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs[FB_GBUFFER_ASSEMBLER]) == VK_SUCCESS);
	}

	//ssao map gen
	{
		using ATT = rp_create_info<RP_SSAO_MAP_GEN>::ATT;

		VkImageView atts[ATT::ATT_COUNT];
		atts[ATT::ATT_SSAO_MAP] = m_res_image[RES_IMAGE_SSAO_MAP].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT::ATT_COUNT;
		fb.pAttachments = atts;
		fb.height = SWAP_CHAIN_IMAGE_EXTENT.height;
		fb.width = SWAP_CHAIN_IMAGE_EXTENT.width;
		fb.layers = 1;
		fb.renderPass = m_rps[RP_SSAO_MAP_GEN];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs[FB_SSAO_MAP_GEN]) == VK_SUCCESS);
	}

	//preimage assembler
	{
		using ATT = rp_create_info<RP_PREIMAGE_ASSEMBLER>::ATT;

		VkImageView atts[ATT::ATT_COUNT];
		atts[ATT::ATT_METALNESS_SSDS] = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].view;
		atts[ATT::ATT_BASECOLOR_SSAO] = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].view;
		atts[ATT::ATT_PREIMAGE] = m_res_image[RES_IMAGE_PREIMAGE].view;
		atts[ATT::ATT_DEPTHSTENCIL] = m_res_image[RES_IMAGE_GB_DEPTH].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT::ATT_COUNT;
		fb.pAttachments = atts;
		fb.height = SWAP_CHAIN_IMAGE_EXTENT.height;
		fb.width = SWAP_CHAIN_IMAGE_EXTENT.width;
		fb.layers = 1;
		fb.renderPass = m_rps[RP_PREIMAGE_ASSEMBLER];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs[FB_PREIMAGE_ASSEMBLER]) == VK_SUCCESS);
	}

	//refraction map gen
	{
		using ATT = rp_create_info<RP_REFRACTION_IMAGE_GEN>::ATT;

		VkImageView atts[ATT::ATT_COUNT];
		atts[ATT::ATT_DEPTHSTENCIL] = m_res_image[RES_IMAGE_GB_DEPTH].view;
		atts[ATT::ATT_REFRACTION_IMAGE] = m_res_image[RES_IMAGE_REFRACTION_IMAGE].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT::ATT_COUNT;
		fb.pAttachments = atts;
		fb.height = SWAP_CHAIN_IMAGE_EXTENT.height;
		fb.width = SWAP_CHAIN_IMAGE_EXTENT.width;
		fb.layers = 1;
		fb.renderPass = m_rps[RP_REFRACTION_IMAGE_GEN];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs[FB_REFRACTION_IMAGE_GEN]) == VK_SUCCESS);
	}

	//water drawer
	{
		using ATT = rp_create_info<RP_WATER_DRAWER>::ATT;

		VkImageView atts[ATT::ATT_COUNT];
		atts[ATT::ATT_FRAME_IMAGE] = m_res_image[RES_IMAGE_PREIMAGE].view;
		atts[ATT::ATT_DEPTHSTENCIL] = m_res_image[RES_IMAGE_GB_DEPTH].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT::ATT_COUNT;
		fb.pAttachments = atts;
		fb.height = SWAP_CHAIN_IMAGE_EXTENT.height;
		fb.width = SWAP_CHAIN_IMAGE_EXTENT.width;
		fb.layers = 1;
		fb.renderPass = m_rps[RP_WATER_DRAWER];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs[FB_WATER_DRAWER]) == VK_SUCCESS);
	}

	//postprocessing
	{
		using ATT = rp_create_info<RP_POSTPROCESSING>::ATT;

		VkImageView atts[ATT::ATT_COUNT];

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT::ATT_COUNT;
		fb.pAttachments = atts;
		fb.height = SWAP_CHAIN_IMAGE_EXTENT.height;
		fb.width = SWAP_CHAIN_IMAGE_EXTENT.width;
		fb.layers = 1;
		fb.renderPass = m_rps[RP_POSTPROCESSING];

		for (uint32_t i = 0; i<SWAP_CHAIN_IMAGE_COUNT; ++i)
		{
			atts[ATT::ATT_SWAP_CHAIN_IMAGE] = m_base.swapchain_views[i];
			atts[ATT::ATT_PREV_IMAGE] = m_res_image[RES_IMAGE_PREV_IMAGE].view;

			assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_postprocessing_fbs[i]) == VK_SUCCESS);
		}
	}
}