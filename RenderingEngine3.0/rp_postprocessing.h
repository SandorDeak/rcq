#pragma once

#include "rp_create_info.h"
#include <array>

namespace rcq
{
	template<>
	struct rp_create_info<RP_POSTPROCESSING>
	{
		enum ATT : uint32_t
		{
			ATT_SWAP_CHAIN_IMAGE,
			ATT_PREV_IMAGE,
			ATT_COUNT
		};
		enum DEP : uint32_t
		{
			DEP_BEGIN,
			DEP_COUNT
		};

		template<uint32_t subpass_id>
		struct subpass;

		template<>
		struct subpass<0>
		{
			static constexpr std::array<VkAttachmentReference, 2> ref_colors = 
			{
				ATT_SWAP_CHAIN_IMAGE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

				ATT_PREV_IMAGE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
		};

		static constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts =
		{
			//ATT_SWAP_CHAIN_IMAGE
			0,
			VK_FORMAT_B8G8R8A8_UNORM,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,

			//ATT_PREV_IMAGE
			0,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		static constexpr std::array<VkSubpassDependency, DEP_COUNT> deps =
		{
			VK_SUBPASS_EXTERNAL,
			0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			0
		};
		static constexpr VkSubpassDescription subpass_unique =
		{
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			subpass<0>::ref_colors.size(),
			subpass<0>::ref_colors.data(),
			nullptr,
			nullptr,
			0,
			nullptr
		};
		static constexpr VkRenderPassCreateInfo create_info =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			nullptr,
			0,
			atts.size(),
			atts.data(),
			1,
			&subpass_unique,
			deps.size(),
			deps.data()
		};
	};
}