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

		static void init(const base_info& info, const renderable_container& rends);
		static void destroy();
		static gta5_pass* instance() { return m_instance; }

		void render(const render_settings& settings, std::bitset<RENDERABLE_TYPE_COUNT> record_mask);
		void wait_for_finish();

	private:
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
			MEMORY_PREIMAGE,
			MEMORY_COUNT
		};

		enum SAMPLER
		{
			SAMPLER_UNNORMALIZED_COORD,
			SAMPLER_GENERAL,
			SAMPLER_COUNT
		};

		enum SECONDARY_CB
		{
			SECONDARY_CB_MAT_EM,
			SECONDARY_CB_SKYBOX_EM,
			SECONDARY_CB_DIR_SHADOW_GEN,
			SECONDARY_CB_MAT_OPAQUE,
			SECONDARY_CB_COUNT
		};

		static const uint32_t ENVIRONMENT_MAP_SIZE = 128;
		static const uint32_t DIR_SHADOW_MAP_SIZE = 1024;
		static const uint32_t FRUSTUM_SPLIT_COUNT = 2;

		enum RENDER_PASS
		{
			RENDER_PASS_ENVIRONMENT_MAP_GEN,
			RENDER_PASS_DIR_SHADOW_MAP_GEN,
			RENDER_PASS_FRAME_IMAGE_GEN,
			RENDER_PASS_POSTPROCESSING,
			RENDER_PASS_COUNT
		};

		enum GP
		{
			GP_ENVIRONMENT_MAP_GEN_MAT,
			GP_ENVIRONMENT_MAP_GEN_SKYBOX,

			GP_DIR_SHADOW_MAP_GEN,

			GP_GBUFFER_GEN,
			GP_SS_DIR_SHADOW_MAP_GEN,
			GP_SS_DIR_SHADOW_MAP_BLUR,
			GP_SSAO_GEN,
			GP_SSAO_BLUR,
			GP_IMAGE_ASSEMBLER,

			GP_POSTPROCESSING,
			GP_COUNT
		};

		struct pipeline
		{
			VkPipeline gp;
			VkPipelineLayout pl;
			VkDescriptorSet ds;
			VkDescriptorSetLayout dsl;
			void create_layout(VkDevice device, const std::vector<VkDescriptorSetLayout>& dsls)
			{
				std::vector<VkDescriptorSet> all_dsl(1 + dsls.size());
				all_dsl[0] = dsl;
				std::copy(dsls.begin(), dsls.end(), all_dsl.begin() + 1);
				pl = rcq::create_layout(device, all_dsl);
			}
			void create_dsl(VkDevice device, const VkDescriptorSetLayoutCreateInfo& create_info)
			{
				if (vkCreateDescriptorSetLayout(device, &create_info, host_memory_manager, &dsl) != VK_SUCCESS)
					throw std::runtime_error("failed to create dsl!");
			}
			void bind(VkCommandBuffer cb)
			{
				vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gp);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pl,
					0, 2, &ds, 0, nullptr);
			}
		};

		struct res_image
		{
			VkImage image;
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

		struct framebuffers
		{
			VkFramebuffer environment_map_gen_fb;
			VkFramebuffer dir_shadow_map_gen_fb;
			VkFramebuffer frame_image_gen;
			std::vector<VkFramebuffer> postprocessing;
		};


		struct res_data
		{
			std::tuple<
				environment_map_gen_mat_data*,
				environment_map_gen_skybox_data*,
				gbuffer_gen_data*,
				dir_shadow_map_gen_data*,
				ss_dir_shadow_map_gen_data*,
				image_assembler_data*
			> data;

			VkBuffer buffer;
			VkBuffer staging_buffer;
			VkDeviceMemory staging_buffer_mem;
			size_t size;
			std::array<size_t, RES_DATA_COUNT> offsets;

			static constexpr std::array<size_t, RES_DATA_COUNT> get_sizes()
			{
				return {
					sizeof(environment_map_gen_mat_data),
					sizeof(environment_map_gen_skybox_data),
					sizeof(gbuffer_gen_data),
					sizeof(dir_shadow_map_gen_data),
					sizeof(ss_dir_shadow_map_gen_data),
					sizeof(image_assembler_data) };
			}

			void calcoffset_and_size(size_t alignment)
			{
				constexpr auto res_data_sizes = res_data::get_sizes();
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
				auto l = { ([](auto&& p, char* val) {
					p = reinterpret_cast<std::remove_reference_t<decltype(p)>>(val);
				}(std::get<indices>(data), base + std::get<indices>(offsets))
					, 0)... };
			}
			template<typename T>
			auto get()
			{
				return std::get<T*>(data);
			}
		};

		gta5_pass(const base_info& info, const renderable_container& rends);

		static gta5_pass* m_instance;
		const base_info m_base;

		void create_render_passes();
		void create_dsls_and_allocate_dss();
		void create_graphics_pipelines();
		void send_memory_requirements();
		void get_memory_and_build_resources();
		void update_descriptor_sets();
		void create_command_pool();
		void create_samplers();
		void create_framebuffers();
		void allocate_command_buffers();
		void record_present_command_buffers();
		void process_settings(const render_settings& settings);
		std::array<glm::mat4, FRUSTUM_SPLIT_COUNT> calc_projs(const render_settings& settings);
		void create_sync_objects();

		std::array<VkRenderPass, GP_COUNT> m_passes;
		std::array<pipeline, GP_COUNT> m_gps;

		//resources
		VkDescriptorPool m_dp;
		res_data m_res_data;
		std::array<res_image, RES_IMAGE_COUNT> m_res_image;
		std::array<VkSampler, SAMPLER_COUNT> m_samplers;

		//render
		const renderable_container& m_renderables;

		VkCommandPool m_cp;
		framebuffers m_fbs;
		VkCommandBuffer m_render_cb;
		std::vector<VkCommandBuffer> m_present_cbs;
		std::array<VkCommandBuffer, SECONDARY_CB_COUNT> m_secondary_cbs;
		VkSemaphore m_image_available_s;
		VkSemaphore m_render_finished_s;
		std::vector<VkSemaphore> m_present_ready_ss;
		VkFence m_render_finished_f;
	};
}

