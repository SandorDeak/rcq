#pragma once
#include "foundation.h"
/*#include "basic_pass.h"
#include "omni_light_shadow_pass.h"*/


namespace rcq
{

	class core
	{
	public:
		core(const core&) = delete;
		core(core&&) = delete;
		~core();

		static void init();
		static void destroy();
		static core* instance() { return m_instance; }

		void push_package(std::unique_ptr<core_package>&& package)
		{
			/*timer t;
			t.start();*/
			std::unique_lock<std::mutex> lock(m_package_mutex);
			if (m_package)
				m_package_condvar.wait(lock, [this]() { return !m_package; });
			m_package = std::move(package);
			/*t.stop();
			std::cout << "core push package wait: " << t.get() << "\n";*/
		}

		renderable_container& get_renderable_container() { return m_renderables; }

	private:
		core();
		void loop();
		bool m_should_end;
		static core* m_instance;

		std::thread m_looping_thread;

		std::mutex m_package_mutex;
		std::condition_variable m_package_condvar;
		std::unique_ptr<core_package> m_package;

		renderable_container m_renderables;

		render_settings m_render_settings;
		
		/*template<size_t... render_passes>
		void record_and_render(const std::optional<camera_data>& cam, std::bitset<RENDERABLE_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_mask,
			std::index_sequence<render_passes...>)
		{
			auto l = { (render_pass_typename<render_passes>::type::instance()->record_and_render(cam, record_mask), 0)... };
		}*/
		/*void record_and_render(const glm::mat4& view, const std::optional<update_proj>& proj_info, 
			std::bitset<RENDERABLE_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_mask);
		VkSemaphore m_begin_s;

		template<size_t... render_passes>
		void wait_for_finish(std::index_sequence<render_passes...>)
		{
			auto l = { (render_pass_typename<render_passes>::type::instance()->wait_for_finish(), 0)... }; 
		}*/

		template<size_t... renderable_types>
		void build_renderables(const std::array<std::vector<build_renderable_info>, 
			RENDERABLE_TYPE_COUNT>& build_infos,
			std::index_sequence<renderable_types...>)
		{
			auto l = { (build_renderables_impl<renderable_types>(build_infos[renderable_types]),0)... };
		}

		template<size_t rend_type>
		inline void build_renderables_impl(const std::vector<build_renderable_info>& build_infos);
	};
}

