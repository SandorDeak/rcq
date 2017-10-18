//#pragma once
//#include "foundation.h"
//
//namespace rcq
//{
//	class basic_pass
//	{
//	public:
//		basic_pass(const basic_pass&) = delete;
//		basic_pass(basic_pass&&) = delete;
//		~basic_pass();
//
//		static void init(const base_info& info, const renderable_container& renderables);
//		static void destroy();
//		static basic_pass* instance() { return m_instance; }
//
//		VkSemaphore record_and_render(const glm::mat4& view, const std::optional<update_proj>& proj_info,
//			std::bitset<RENDERABLE_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_mask, VkSemaphore wait_s);
//		void wait_for_finish();
//
//	private:
//		enum SUBPASS
//		{
//			SUBPASS_MAT,
//			SUBPASS_LIGHT,
//			SUBPASS_SKYBOX,
//			SUBPASS_COUNT
//		};
//
//		basic_pass(const base_info& info, const renderable_container& renderables);
//		
//		void create_render_pass();
//		void create_descriptors();
//		void create_opaque_mat_pipeline();
//		void create_omni_light_pipeline();
//		void create_skybox_pipeline();
//		void create_dir_shadow_map_pipeline();
//		void create_command_pool();
//		void create_resources();	
//		void create_framebuffers();
//		void allocate_command_buffers();
//		void create_semaphores();
//		void create_fences();
//
//		static basic_pass* m_instance;
//		
//		const base_info m_base;
//		VkRenderPass m_pass;
//
//
//		//resources
//		texture m_depth_tex;
//		VkImage m_gbuffer_image; 
//		VkDeviceMemory m_gbuffer_mem;
//		std::array<VkImageView, 5> m_gbuffer_views;
//
//		std::vector<VkFramebuffer> m_fbs;
//
//		VkCommandPool m_cp;
//
//		std::array<VkPipeline, RENDERABLE_TYPE_COUNT> m_gps;
//		std::array<VkPipelineLayout, RENDERABLE_TYPE_COUNT> m_pls;
//
//		VkDescriptorSetLayout m_gbuffer_dsl;
//		std::array<VkDescriptorSetLayout, RENDERABLE_TYPE_COUNT> m_per_frame_dsls;
//
//		VkDescriptorPool m_dp;
//		std::array<VkDescriptorSet, RENDERABLE_TYPE_COUNT> m_per_frame_dss; 
//		
//		VkBuffer m_per_frame_b;
//		VkDeviceMemory m_per_frame_b_mem;
//		char* m_per_frame_b_data;
//
//		VkDescriptorSet m_gbuffer_ds;
//
//		std::array<std::vector<VkCommandBuffer>, RENDERABLE_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> m_secondary_cbs;
//		std::vector<VkCommandBuffer> m_primary_cbs;
//
//		const renderable_container& m_renderables;
//
//		VkSemaphore m_image_available_s;
//		VkSemaphore m_render_finished_s;
//		VkSemaphore m_render_finished_s2;
//		
//		std::vector<VkFence> m_primary_cb_finished_fs;
//		std::vector<std::bitset<RENDERABLE_TYPE_COUNT*LIFE_EXPECTANCY_COUNT>> m_record_masks;
//
//		//special pipelines
//		VkPipeline m_gp_dir_shadow;
//		VkPipelineLayout m_pl_dir_shadow;
//
//		//cascade shadow map
//		update_proj m_proj;
//
//		void update_frustum_points();
//
//
//	};
//
//}
//
