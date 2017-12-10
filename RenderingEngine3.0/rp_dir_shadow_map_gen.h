#pragma once

#include "rp_create_info.h"
#include <array>

namespace rcq
{
	template<>
	struct rp_create_info<RP_DIR_SHADOW_MAP_GEN>
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

		template<uint32_t subpass_id>
		struct subpass;

		template<>
		struct subpass<0>
		{
			static constexpr VkAttachmentReference ref_depth =
			{
				ATT_DEPTH,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			};
		};

		static constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts =
		{
			0,
			VK_FORMAT_D32_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
		};
		static constexpr std::array<VkSubpassDependency, DEP_COUNT> deps =
		{
			VK_SUBPASS_EXTERNAL,
			0,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
			0,

			0,
			VK_SUBPASS_EXTERNAL,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			0
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
			deps.size(),
			deps.data()
		};
	};
}