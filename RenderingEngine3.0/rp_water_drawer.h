#pragma once

#include "rp_create_info.h"
#include <array>

namespace rcq
{
	template<>
	struct rp_create_info<RP_WATER_DRAWER>
	{
		enum ATT
		{
			ATT_FRAME_IMAGE,
			ATT_DEPTHSTENCIL,
			ATT_COUNT
		};

		template<uint32_t subpass_id>
		struct subpass;

		template<>
		struct subpass<0>
		{
			static constexpr VkAttachmentReference ref_color =
			{
				ATT_FRAME_IMAGE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			static constexpr VkAttachmentReference ref_depth =
			{
				ATT_DEPTHSTENCIL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			};
		};

		static constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts =
		{
			//ATT_FRAME_IMAGE
			0,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

			//ATT_DEPTHSTENCIL
			0,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		};
		static constexpr VkSubpassDescription subpass_unique =
		{
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			1,
			&subpass<0>::ref_color,
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