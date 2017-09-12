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

		static void init(const base_info& info);
		static void destroy();
		static basic_pass* instance() { return m_instance; }

	private:
		basic_pass(const base_info& info);
		
		void create_render_pass();
		void create_descriptor_set_layouts();
		void create_graphics_pipeline();

		static basic_pass* m_instance;
		
		const base_info m_base;
		VkRenderPass m_pass;
		VkDescriptorSetLayout m_cam_dsl;
		VkDescriptorSetLayout m_tr_dsl;

		VkPipelineLayout m_pl;
		VkPipeline m_gp;
	};

}

