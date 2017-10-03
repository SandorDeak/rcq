#pragma once

#include "foundation.h"

namespace rcq
{

	class omni_light_shadow_pass
	{
	public:
		omni_light_shadow_pass(const omni_light_shadow_pass&) = delete;
		omni_light_shadow_pass(omni_light_shadow_pass&&) = delete;
		~omni_light_shadow_pass();

		static void init(const base_info& info, const renderable_container& renderables,
			const light_renderable_container& light_renderables);
		static void destroy();
		static omni_light_shadow_pass* instance() { return m_instance; }

		void record_and_render(const std::optional<camera_data>& cam, std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> mat_record_mass,
			std::bitset<LIGHT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> light_record_mask);
		void wait_for_finish();

		void create_framebuffer(light& l);

	private:
		omni_light_shadow_pass(const base_info& info, const renderable_container& renderables,
			const light_renderable_container& light_renderables);

		void create_render_pass();
		void create_command_pool();
		void create_pipeline();
		void allocate_command_buffer();
		void create_semaphore();
		void create_fence();


		static omni_light_shadow_pass* m_instance;

		const renderable_container& m_renderables;
		const light_renderable_container& m_light_renderables;
		const base_info m_base;

		VkRenderPass m_pass;
		VkCommandPool m_cp;
		VkPipelineLayout m_pl;
		VkPipeline m_gp;

		VkCommandBuffer m_cb;

		VkSemaphore m_render_finished_s;
		VkFence m_render_finished_f;
	};
}
