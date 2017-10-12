#pragma once
#include "foundation.h"

namespace rcq::gta5
{
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
		ATTACHMENT_GB_POS_ROUGHNESS,
		ATTACHMENT_GB_F0_SSAO,
		ATTACHMENT_GB_ALBEDO_SSDS,
		ATTACHMENT_GB_NORMAL_AO,
		ATTACHMENT_GB_DEPTH,
		ATTACHMENT_EM_COLOR,
		ATTACHMENT_EM_DEPTH,
		ATTACHMENT_DS_MAP,
		ATTACHMENT_SSDS_MAP,
		ATTACHMENT_SSAO_MAP,
		ATTACHMENT_PREIMAGE,
		ATTACHMENT_COUNT
	};

	enum RES_DATA
	{
		RES_DATA_ENVIRONMENT_MAP_GEN_MAT_DATA,
		RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX_DATA,
		RES_DATA_GBUFFER_GEN_DATA,
		RES_DATA_DIR_SHADOW_MAP_GEN_DATA,
		RES_DATA_SS_DIR_SHADOW_MAP_GEN_DATA,
		RES_DATA_IMAGE_ASSEMBLER_DATA,
		RES_DATA_COUNT
	};
	enum RES_IMAGE
	{
		RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL,
		RES_IMAGE_ENVIRONMENT_MAP,
		RES_IMAGE_GB_POS_ROUGHNESS,
		RES_IMAGE_GB_F0_SSAO,
		RES_IMAGE_GB_ALBEDO_SSDS,
		RES_IMAGE_GB_NORMAL_AO,
		RES_IMAGE_GB_DEPTH,
		RES_IMAGE_DIR_SHADOW_MAP,
		RES_IMAGE_SS_DIR_SHADOW_MAP,
		RES_IMAGE_SSAO_MAP,
		RES_IMAGE_PREIMAGE,
		RES_IMAGE_COUNT
	};


	enum MEMORY
	{
		MEMORY_STAGING_BUFFER,
		MEMORY_BUFFER,
		MEMORY_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL,
		MEMORY_ENVIRONMENT_MAP,
		MEMORY_GB_POS_ROUGHNESS,
		MEMORY_GB_F0_SSAO,
		MEMORY_GB_ALBEDO_SSDS,
		MEMORY_GB_NORMAL_AO,
		MEMORY_GB_DEPTH,
		MEMORY_DIR_SHADOW_MAP,
		MEMORY_SS_DIR_SHADOW_MAP,
		MEMORY_SSAO_MAP,
		//MEMORY_PREIMAGE,
		MEMORY_COUNT
	};

	const uint32_t ENVIRONMENT_MAP_SIZE = 128;
	const uint32_t DIR_SHADOW_MAP_SIZE = 1024;
	const uint32_t FRUSTUM_SPLIT_COUNT = 2;

	struct pipeline
	{
		VkPipeline gp;
		VkPipelineLayout pl;
		VkDescriptorSet ds;
		VkDescriptorSetLayout dsl;
	};

	struct res_image
	{
		VkImage image;
		VkImage memory;
		VkImageView view;
	};

	struct environment_map_gen_mat_data
	{
		glm::mat4 view;
		glm::vec3 dir; //in worlds space
		uint32_t padding0;
		glm::vec3 irradiance;
		uint32_t padding1;
		glm::vec3 ambient_irradiance;
		uint32_t padding2;
	};
	struct environment_map_gen_skybox_data
	{
		glm::mat4 view;
	};
	struct gbuffer_gen_data
	{
		glm::mat4 proj;
		glm::mat4 view;
	};
	struct dir_shadow_map_gen_data
	{
		glm::mat4 projs[FRUSTUM_SPLIT_COUNT];
	};
	struct ss_dir_shadow_map_gen_data
	{
		glm::mat4 projs[FRUSTUM_SPLIT_COUNT];
		float near;
		float far;
	};
	struct image_assembler_data
	{
		glm::vec3 dir; //in view space
		uint32_t padding0;
		glm::vec3 irradiance;
		uint32_t padding1;
		glm::vec3 ambient_irradiance;
		uint32_t padding2;
	};

	struct res_data
	{
		std::tuple<
			environment_map_gen_mat_data*,
			environment_map_gen_mat_data*,
			gbuffer_gen_data*,
			dir_shadow_map_gen_data*,
			ss_dir_shadow_map_gen_data*,
			image_assembler_data*
		> data;

		VkBuffer buffer;
		VkDeviceMemory buffer_mem;
		VkBuffer staging_buffer;
		VkDeviceMemory staging_buffer_mem;
		size_t size;
		std::array<size_t, RES_DATA_COUNT> offsets;

		static constexpr std::array<size_t, RES_DATA_COUNT> get_sizes(std::index_sequence<indices...>)
		{
			return { 
				sizeof(environment_map_gen_mat_data),
				sizeof(environment_map_gen_mat_data),
				sizeof(gbuffer_gen_data),
				sizeof(dir_shadow_map_gen_data),
				sizeof(ss_dir_shadow_map_gen_data),
				sizeof(image_assembler_data) };
		}

		void calcoffset_and_size(size_t alignment)
		{
			constexpr auto res_data_sizes = res_data::get_sizes(std::make_index_sequence<RES_DATA_COUNT>());
			size = 0;
			for (uint32_t i = 0; i < RES_DATA_COUNT; ++i)
			{
				offsets[i] = size;
				size += calc_offset(alignment, res_data_sizes[i]);
			}
		}

		template<size_t... indices>
		void set_pointers(char* base, std::index_sequence<indices...>)
		{
			auto l = { [](auto&& p, char* val) {
				p = reinterpret_cast<std::remove_reference_t(decltype(p))>(val);
			}(std::get<indices>(data), base+std::get<indices>(offsets))
			, 0)... };
		}
	};


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

		gta5_pass(const base_info& info);

		static gta5_pass* m_instance;
		const base_info m_base;

		void create_render_pass();
		void create_descriptos_set_layouts();
		void create_graphics_pipelines();
		void send_memory_requirements();
		void get_memory_and_build_resources();
		void create_descriptor_sets();

		VkRenderPass m_pass;
		std::array<pipeline, GP_COUNT> m_gps;

		//resources
		res_data m_res_data;
		std::array<res_image, RES_IMAGE_COUNT> m_res_image;

		//command buffers
		VkCommandPool m_cp;
	};
}

