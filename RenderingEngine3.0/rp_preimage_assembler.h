#pragma once

#include "rp_create_info.h"
#include <array>

namespace rcq
{
	template<>
	struct rp_create_info<RP_PREIMAGE_ASSEMBLER>
	{
		enum ATT : uint32_t
		{
			ATT_BASECOLOR_SSAO,
			ATT_METALNESS_SSDS,
			ATT_PREIMAGE,
			ATT_DEPTHSTENCIL,
			ATT_COUNT
		};

		enum SUBPASS : uint32_t
		{
			SUBPASS_SS_DIR_SHADOW_MAP_BLUR,
			SUBPASS_SSAO_BLUR,
			SUBPASS_SSR_RAY_CASTING,
			SUBPASS_IMAGE_ASSEMBLER,
			SUBPASS_SKY_DRAWER,
			SUBPASS_SUN_DRAWER,
			SUBPASS_COUNT
		};

		enum DEP : uint32_t
		{
			DEP_SSDS_BLUR_IMAGE_ASSEMBLER,
			DEP_SSAO_BLUR_IMAGE_ASSEMBLER,
			DEP_SSR_RAY_CASTING_IMAGE_ASSEMBLER,
			DEP_IMAGE_ASSEMBLER_SKY_DRAWER,
			DEP_SKY_DRAWER_SUN_DRAWER,
			DEP_COUNT
		};

		template<uint32_t subpass_id>
		struct subpass;

		template<>
		struct subpass<SUBPASS_SSR_RAY_CASTING>
		{
			static constexpr VkAttachmentReference ref_depth = 
			{
				ATT_DEPTHSTENCIL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
			};
		};
		template<>
		struct subpass<SUBPASS_SUN_DRAWER>
		{
			static constexpr VkAttachmentReference ref_color = 
			{
				ATT_PREIMAGE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			static constexpr VkAttachmentReference ref_depth =
			{
				ATT_DEPTHSTENCIL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
			};
		};
		template<>
		struct subpass<SUBPASS_SKY_DRAWER>
		{
			static constexpr VkAttachmentReference ref_color =
			{
				ATT_PREIMAGE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			static constexpr VkAttachmentReference ref_depth =
			{
				ATT_DEPTHSTENCIL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
			};
		};
		template<>
		struct subpass<SUBPASS_SS_DIR_SHADOW_MAP_BLUR>
		{
			static constexpr VkAttachmentReference ref_color =
			{
				ATT_METALNESS_SSDS,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
		};
		template<>
		struct subpass<SUBPASS_SSAO_BLUR>
		{
			static constexpr VkAttachmentReference ref_color =
			{
				ATT_BASECOLOR_SSAO,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			static constexpr uint32_t ref_pres = ATT_METALNESS_SSDS;
		};
		template<>
		struct subpass<SUBPASS_IMAGE_ASSEMBLER>
		{
			static constexpr std::array<VkAttachmentReference, 2> ref_inputs =
			{
				ATT_BASECOLOR_SSAO,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

				ATT_METALNESS_SSDS,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			static constexpr VkAttachmentReference ref_depth =
			{
				ATT_DEPTHSTENCIL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
			};
			static constexpr VkAttachmentReference ref_color =
			{
				ATT_PREIMAGE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
		};

		static constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts =
		{
			//ATT_BASECOLOR_SSAO
			0,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

			//ATT_METALNESS_SSDS
			0,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

			//ATT_PREIMAGE
			0,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

			//ATT_DEPTHSTENCIL
			0,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
		};
		static constexpr std::array<VkSubpassDependency, DEP_COUNT> deps =
		{
			//DEP_SSDS_BLUR_IMAGE_ASSEMBLER
			SUBPASS_SS_DIR_SHADOW_MAP_BLUR,
			SUBPASS_IMAGE_ASSEMBLER,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
			0,

			//DEP_SSAO_BLUR_IMAGE_ASSEMBLER
			SUBPASS_SSAO_BLUR,
			SUBPASS_IMAGE_ASSEMBLER,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
			0,

			//DEP_SSR_RAY_CASTING_IMAGE_ASSEMBLER
			SUBPASS_SSR_RAY_CASTING,
			SUBPASS_IMAGE_ASSEMBLER,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			0,

			//DEP_IMAGE_ASSEMBLER_SKY_DRAWER
			SUBPASS_IMAGE_ASSEMBLER,
			SUBPASS_SKY_DRAWER,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			0,

			//DEP_SKY_DRAWER_SUN_DRAWER
			SUBPASS_SKY_DRAWER,
			SUBPASS_SUN_DRAWER,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			0
		};

		static constexpr std::array<VkSubpassDescription, SUBPASS_COUNT> subpasses =
		{
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			1,
			&subpass<0>::ref_color,
			nullptr,
			nullptr,
			0,
			nullptr,

			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			1,
			&subpass<1>::ref_color,
			nullptr,
			nullptr,
			1,
			&subpass<1>::ref_pres,

			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			0,
			nullptr,
			nullptr,
			&subpass<2>::ref_depth,
			0,
			nullptr,
			

			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			subpass<3>::ref_inputs.size(),
			subpass<3>::ref_inputs.data(),
			1,
			&subpass<3>::ref_color,
			nullptr,
			&subpass<3>::ref_depth,
			0,
			nullptr,

			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			1,
			&subpass<4>::ref_color,
			nullptr,
			&subpass<4>::ref_depth,
			0,
			nullptr,

			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			1,
			&subpass<5>::ref_color,
			nullptr,
			&subpass<5>::ref_depth,
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
			subpasses.size(),
			subpasses.data(),
			deps.size(),
			deps.data()
		};
	};
}