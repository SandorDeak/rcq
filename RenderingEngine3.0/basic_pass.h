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

		static void init(const base_info& info, const renderable_container& renderables);
		static void destroy();
		static basic_pass* instance() { return m_instance; }

		void record_and_render(const std::optional<camera_data>& cam, std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_maks);
		void wait_for_finish();

	private:
		basic_pass(const base_info& info, const renderable_container& renderables);
		
		void create_render_pass();
		void create_per_frame_resources();
		void create_graphics_pipelines();
		void create_command_pool();
		void create_depth_and_per_frame_buffer();	
		void create_framebuffers();
		void allocate_command_buffers();
		void create_semaphores();
		void create_fences();

		static basic_pass* m_instance;
		
		const base_info m_base;
		VkRenderPass m_pass;

		texture m_depth_tex;

		std::vector<VkFramebuffer> m_fbs;
		VkCommandPool m_cp;

		std::array<VkPipelineLayout, MAT_TYPE_COUNT> m_pls;
		std::array<VkPipeline, MAT_TYPE_COUNT> m_gps;
		std::array<VkDescriptorSetLayout, MAT_TYPE_COUNT> m_per_frame_dsls;
		std::array<VkDescriptorSet, MAT_TYPE_COUNT> m_per_frame_dss;
		VkDescriptorPool m_per_frame_dp;
		VkBuffer m_per_frame_b;
		VkDeviceMemory m_per_frame_b_mem;
		char* m_per_frame_b_data;

		std::array<std::vector<VkCommandBuffer>, MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> m_secondary_cbs;
		std::vector<VkCommandBuffer> m_primary_cbs;

		const renderable_container& m_renderables;


		VkSemaphore m_image_available_s;
		VkSemaphore m_render_finished_s;
		
		std::vector<VkFence> m_primary_cb_finished_fs;
		std::vector<std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT>> m_record_masks;

	};

}

