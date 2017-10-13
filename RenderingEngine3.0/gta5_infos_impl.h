#pragma once

#include "foundation.h"

namespace rcq::gta5::infos_impl
{
	
	struct render_pass_environment_map_gen
	{
		enum ATT
		{
			ATT_DEPTH,
			ATT_COLOR,
			ATT_COUNT
		};

		enum SUBPASS
		{
			SUBPASS_UNIQUE,
			SUBPASS_COUNT
		};

		enum DEP
		{
			DEP_BEGIN,
			DEP_END,
			DEP_COUNT
		};

		struct subpass_unique
		{
			subpass_unique()
			{
				ref_color.attachment = ATT_COLOR;
				ref_color.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				ref_depth.attachment = ATT_DEPTH;
				ref_depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}

			VkSubpassDescription create_subpass()
			{
				VkSubpassDescription subpass = {};
				subpass.colorAttachmentCount = 1;
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.pColorAttachments = &ref_color;
				subpass.pDepthStencilAttachment = &ref_depth;
				return subpass;
			}

			VkAttachmentReference ref_color;
			VkAttachmentReference ref_depth;
		};

		static constexpr std::array<VkAttachmentDescription, ATT_COUNT> get_attachments()
		{

			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};
			atts[ATT_DEPTH].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_DEPTH].finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_DEPTH].format = VK_FORMAT_D32_SFLOAT;
			atts[ATT_DEPTH].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_DEPTH].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			atts[ATT_DEPTH].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_DEPTH].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_DEPTH].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			atts[ATT_COLOR].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_COLOR].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			atts[ATT_COLOR].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_COLOR].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_COLOR].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_COLOR].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_COLOR].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_COLOR].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			return atts;
		}

		static constexpr auto get_dependencies()
		{
			std::array<VkSubpassDependency, DEP_COUNT> d = {};
			d[DEP_BEGIN].srcSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_BEGIN].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_BEGIN].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			d[DEP_BEGIN].dstSubpass = SUBPASS_UNIQUE;
			d[DEP_BEGIN].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_BEGIN].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			d[DEP_END].srcSubpass = SUBPASS_UNIQUE;
			d[DEP_END].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_END].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_END].dstSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_END].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_END].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			return d;
		}

		std::array<VkSubpassDependency, DEP_COUNT> deps;
		std::array<VkAttachmentDescription, ATT_COUNT> atts;
		subpass_unique sp_unique;
		std::array<VkSubpassDescription, SUBPASS_COUNT> subpasses;
		VkRenderPassCreateInfo create_info;
	public:
		render_pass_environment_map_gen():
			deps(get_dependencies()),
			atts(get_attachments())
		{
			subpasses[SUBPASS_UNIQUE] = sp_unique.create_subpass();

			create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			create_info.attachmentCount = ATT_COUNT;
			create_info.dependencyCount = DEP_COUNT;
			create_info.pAttachments = atts.data();
			create_info.subpassCount = SUBPASS_COUNT;
			create_info.pSubpasses = subpasses.data();
			create_info.pDependencies = deps.data();
		}

		VkRenderPassCreateInfo* get_create_info() { return &create_info; }
	};


	struct render_pass_dir_shadow_map_gen
	{
		enum ATT
		{
			ATT_DEPTH,
			ATT_COUNT
		};

		enum DEP
		{
			DEP_BEGIN,
			DEP_END,
			DEP_COUNT
		};

		enum SUBPASS
		{
			SUBPASS_UNIQUE,
			SUBPASS_COUNT
		};

		struct subpass_unique
		{
			subpass_unique()
			{
				ref_depth.attachment = ATT_DEPTH;
				ref_depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}

			VkSubpassDescription create_subpass()
			{
				VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.pDepthStencilAttachment = &ref_depth;
				return subpass;
			}

			VkAttachmentReference ref_depth;
		};

		static constexpr std::array<VkAttachmentDescription, ATT_COUNT> get_attachments()
		{

			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};
			atts[ATT_DEPTH].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_DEPTH].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			atts[ATT_DEPTH].format = VK_FORMAT_D32_SFLOAT;
			atts[ATT_DEPTH].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_DEPTH].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			atts[ATT_DEPTH].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_DEPTH].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_DEPTH].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			return atts;
		}

		static constexpr auto get_dependencies()
		{
			std::array<VkSubpassDependency, DEP_COUNT> d = {};
			d[DEP_BEGIN].srcSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_BEGIN].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_BEGIN].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			d[DEP_BEGIN].dstSubpass = SUBPASS_UNIQUE;
			d[DEP_BEGIN].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			d[DEP_BEGIN].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

			d[DEP_END].srcSubpass = SUBPASS_UNIQUE;
			d[DEP_END].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			d[DEP_END].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			d[DEP_END].dstSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_END].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_END].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			return d;
		}

		std::array<VkSubpassDependency, DEP_COUNT> deps;
		std::array<VkAttachmentDescription, ATT_COUNT> atts;
		subpass_unique sp_unique;
		std::array<VkSubpassDescription, SUBPASS_COUNT> subpasses;
		VkRenderPassCreateInfo create_info;
	public:
		render_pass_dir_shadow_map_gen() :
			deps(get_dependencies()),
			atts(get_attachments())
		{
			subpasses[SUBPASS_UNIQUE] = sp_unique.create_subpass();

			create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			create_info.attachmentCount = ATT_COUNT;
			create_info.dependencyCount = DEP_COUNT;
			create_info.pAttachments = atts.data();
			create_info.subpassCount = SUBPASS_COUNT;
			create_info.pSubpasses = subpasses.data();
			create_info.pDependencies = deps.data();
		}

		VkRenderPassCreateInfo* get_create_info() { return &create_info; }
	};
	


	struct render_pass_frame_image_gen
	{
		enum ATT
		{
			ATT_DEPTHSTENCIL,
			ATT_POS_ROUGHNESS,
			ATT_F0_SSAO,
			ATT_ALBEDO_SSDS,
			ATT_SS_DIR_SHADOW_MAP,
			ATT_SSAO_MAP,
			ATT_PREIMAGE,
			ATT_COUNT
		};

		enum SUBPASS
		{
			SUBPASS_GBUFFER_GEN,
			SUBPASS_SS_DIR_SHADOW_MAP_GEN,
			SUBPASS_SS_DIR_SHADOW_MAP_BLUR,
			SUBPASS_SSAO_MAP_GEN,
			SUBPASS_SSAO_MAP_BLUR,
			SUBPASS_COUNT
		};

		enum DEP
		{
			DEP_COUNT
		};

		struct subpass_gbuffer_gen
		{

			std::array<VkAttachmentReference, 4> refs_color;
			VkAttachmentDescription ref_dept;
		};
	};
}
