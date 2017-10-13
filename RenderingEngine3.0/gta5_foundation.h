#pragma once

#include "gta5_infos_impl.h"

namespace rcq::gta5
{
	enum
	{
		RENDER_PASS_ENVIRONMENT_MAP_GEN,
		RENDER_PASS_DIR_SHADOW_MAP_GEN,
		RENDER_PASS_FRAME_IMAGE_GEN,
		RENDER_PASS_COUNT
	};

	struct RENDER_PASS_ENVIRONMENT_MAP_GEN_SUBPASS_UNIQUE
	{
		static constexpr auto refs_color = infos_impl::RENDER_PASS_ENVIRONMENT_MAP_GEN_SUBPASS_UNIQUE_IMPL::refs_color();
		static constexpr auto ref_depth = infos_impl::RENDER_PASS_ENVIRONMENT_MAP_GEN_SUBPASS_UNIQUE_IMPL::ref_depth();
	};

	struct RENDER_PASS_ENVIRONMENT_MAP_GEN
	{
		static constexpr auto atts = infos_impl::RENDER_PASS_ENVIRONMENT_MAP_GEN_IMPL::attachments();
		static constexpr auto deps = infos_impl::RENDER_PASS_ENVIRONMENT_MAP_GEN_IMPL::dependencies();
	};

	
	
	

	class DIR_SHADOW_MAP_GEN
	{
		enum 
		{
			DEPTH,
			COUNT
		};
		struct SUBPASS_UNIQUE
		{
			static constexpr auto ref_deph()
			{
				VkAttachmentReference ref = {};
				ref.attachment = DEPTH;
				ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				return ref;
			}
		};
	}

	struct FRAME_IMAGE_GEN
	{
		enum { ID = 2, SUBPASS_COUNT = 6 };
		enum ATT
		{
			ATT_GB_DEPTHSTENCIL,
			ATT_GB_POS_ROUGHNESS,
			ATT_GB_F0_SSAO,
			ATT_GB_ALBEDO_SSDS,
			ATT_SS_DIR_SHADOW_MAP,
			ATT_SSAO_MAP,
			ATT_PREIMAGE,
			ATT_COUNT
		};
		struct SUBPASS_GBUFFER_GEN
		{
			enum { ID = 0 };
			enum REF
			{
				REF_GB_DEPTH_STENCIL,
				REF_GB_POS_ROUGHNESS,
				REF_GB_F0_SSAO,
				REF_GB_ALBEDO_SSDS,
				REF_COUNT
			};
		};
		struct SUBPASS_SS_DIR_SHADOW_MAP_GEN
		{
			enum { ID = 1 };
			enum REF
			{
				REF_
			};
		};
	};
}