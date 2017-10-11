#pragma once
#include "foundation.h"

namespace rcq
{

	class gta5_pass
	{
	public:
		gta5_pass(const gta5_pass&) = delete;
		gta5_pass(gta5_pass&&) = delete;
		~gta5_pass();

		static void init(const base_info& info);
		static void destroy();
		static gta5_pass* instance() { return m_instance; }
	private:
		enum SUBPASS
		{
			SUBPASS_ENVIRONMENT_MAP_GEN,
			SUBPASS_GBUFFER_GEN,
			SUBPASS_DIR_SHADOW_MAP_GEN,
			SUBPASS_SS_DIR_SHADOW_MAP_GEN,
			SUBPASS_SS_DIR_SHADOW_MAP_BLUR,
			SUBPASS_SSAO_GEN,
			SUBPASS_SSAO_BLUR,
			SUBPASS_IMAGE_ASSEMBLER,
			SUBPASS_COUNT
		};

		enum GP
		{
			GP_ENVIRONMENT_MAP_GEN_MAT,
			GP_ENVIRONMENT_MAP_GEN_SKYBOX,
			GP_GBUFFER_GEN,
			GP_DIR_SHADOW_MAP_GEN,
			GP_SS_DIR_SHADOW_MAP_GEN,
			GP_SS_DIR_SHADOW_MAP_BLUR,
			GP_SSAO_GEN,
			GP_SSAO_BLUR,
			GP_IMAGE_ASSEMBLER,
			GP_COUNT
		};

		enum ATTACHMENT
		{
			ATTACHMENT_GB_POS,
			ATTACHMENT_GB_F0_ROUGHNESS,
			ATTACHMENT_GB_ALBEDO_AO,
			ATTACHMENT_GB_NORMAL,
			ATTACHMENT_GB_DEPTH,
			ATTACHMENT_EM_COLOR,
			ATTACHMENT_EM_DEPTH,
			ATTACHMENT_DS_MAP,
			ATTACHMENT_SSDS_MAP,
			ATTACHMENT_SSAO_MAP,
			ATTACHMENT_SSDS_BLURRED_MAP,
			ATTACHMENT_SSAO_BLURRED_MAP,
			ATTACHMENT_PREIMAGE,
			ATTACHMENT_COUNT
		};
		static const uint32_t ENVIRONMENT_MAP_SIZE = 128;


		gta5_pass(const base_info& info);

		static gta5_pass* m_instance;
		const base_info m_base;

		void create_render_pass();
		void create_graphics_pipelines();

		VkRenderPass m_pass;
		std::array<VkPipeline, GP_COUNT> m_gps;
	};
}

