#pragma once

#include "rp_create_info.h"
#include <array>

namespace rcq
{
	template<>
	struct rp_create_info<RP_SSAO_MAP_GEN>
	{
		enum ATT
		{
			ATT_SSAO_MAP,
			ATT_COUNT
		};

		template<uint32_t subpass_id>
		struct subpass;

		template<>
		struct subpass<0>
		{
			static constexpr VkAttachmentReference ref_depth =
			{
				ATT_SSAO_MAP,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			};
		};

		static constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts =
		{
			0,
			VK_FORMAT_D32_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
		};
		static constexpr VkSubpassDescription subpass_unique =
		{
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			0,
			nullptr,
			nullptr,
			&subpass<0>::ref_depth,
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
			0,
			nullptr
		};
	};
}