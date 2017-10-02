#pragma once
#include "foundation.h"

namespace rcq
{
	class basic_pass
	{
	public:
		basic_pass(const basic_pass&) = delete;
		basic_pass(basic_pass&&) = delete;
		~basic_pass();

		static void init(const base_info& info, const renderable_container& renderables, 
			const light_renderable_container& light_renderables);
		static void destroy();
		static basic_pass* instance() { return m_instance; }

		void record_and_render(const std::optional<camera_data>& cam, std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> mat_record_mass,
			std::bitset<LIGHT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> light_record_mask);
		void wait_for_finish();

	private:
		enum SUBPASS
		{
			SUBPASS_MAT,
			SUBPASS_LIGHT
		};

		basic_pass(const base_info& info, const renderable_container& renderables, 
			const light_renderable_container& light_renderables);
		
		void create_render_pass();
		void create_descriptors();
		void create_opaque_mat_pipeline();
		void create_omni_light_pipeline();
		void create_command_pool();
		void create_resources();	
		void create_framebuffers();
		void allocate_command_buffers();
		void create_semaphores();
		void create_fences();

		static basic_pass* m_instance;
		
		const base_info m_base;
		VkRenderPass m_pass;

		texture m_depth_tex;

		VkImage m_gbuffer_image; 
		VkDeviceMemory m_gbuffer_mem;
		std::array<VkImageView, 5> m_gbuffer_views;

		std::vector<VkFramebuffer> m_fbs;

		VkCommandPool m_cp;

		std::array<VkPipeline, MAT_TYPE_COUNT> m_mat_gps;
		std::array<VkPipelineLayout, MAT_TYPE_COUNT> m_mat_pls;

		std::array<VkPipeline, LIGHT_TYPE_COUNT> m_light_gps;
		std::array<VkPipelineLayout, LIGHT_TYPE_COUNT> m_light_pls;

		VkDescriptorSetLayout m_gbuffer_dsl;
		std::array<VkDescriptorSetLayout, MAT_TYPE_COUNT> m_per_frame_dsls;

		VkDescriptorPool m_dp;
		std::array<VkDescriptorSet, MAT_TYPE_COUNT> m_per_frame_dss; 
		
		VkBuffer m_per_frame_b;
		VkDeviceMemory m_per_frame_b_mem;
		char* m_per_frame_b_data;

		VkDescriptorSet m_gbuffer_ds;

		std::array<std::vector<VkCommandBuffer>, MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> m_mat_cbs;
		std::array<std::vector<VkCommandBuffer>, LIGHT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> m_light_cbs;
		std::vector<VkCommandBuffer> m_primary_cbs;

		const renderable_container& m_renderables;
		const light_renderable_container& m_light_renderables;


		VkSemaphore m_image_available_s;
		VkSemaphore m_render_finished_s;
		
		std::vector<VkFence> m_primary_cb_finished_fs;
		std::vector<std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT>> m_mat_record_masks;
		std::vector<std::bitset<LIGHT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT>> m_light_record_masks;

	};

}

