//#pragma once
//
//#include "foundation.h"
//
//namespace rcq
//{
//
//	class omni_light_shadow_pass
//	{
//	public:
//		omni_light_shadow_pass(const omni_light_shadow_pass&) = delete;
//		omni_light_shadow_pass(omni_light_shadow_pass&&) = delete;
//		~omni_light_shadow_pass();
//
//		static void init(const base_info& info, const renderable_container& renderables);
//		static void destroy();
//		static omni_light_shadow_pass* instance() { return m_instance; }
//
//		VkSemaphore record_and_render(const std::optional<camera_data>& cam, 
//			std::bitset<RENDERABLE_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_mask, VkSemaphore wait_s);
//		void wait_for_finish();
//
//		void create_framebuffer(light_omni& l);
//
//	private:
//		omni_light_shadow_pass(const base_info& info, const renderable_container& renderables);
//
//		void create_render_pass();
//		void create_command_pool();
//		void create_pipeline();
//		void allocate_command_buffer();
//		void create_semaphore();
//		void create_fence();
//
//
//		static omni_light_shadow_pass* m_instance;
//
//		const renderable_container& m_renderables;
//		const base_info m_base;
//
//		VkRenderPass m_pass;
//		VkCommandPool m_cp;
//		VkPipelineLayout m_pl;
//		VkPipeline m_gp;
//
//		VkCommandBuffer m_cb;
//
//		VkSemaphore m_render_finished_s;
//		VkFence m_render_finished_f;
//	};
//}
