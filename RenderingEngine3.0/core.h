#pragma once
#include "foundation.h"
#include "basic_pass.h"


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
			std::unique_lock<std::mutex> lock(m_package_queue_mutex);
			if (m_package_queue.size() >= CORE_COMMAND_QUEUE_MAX_SIZE) //blocking
				m_package_queue_condvar.wait(lock, [this]() {return m_package_queue.size() < CORE_COMMAND_QUEUE_MAX_SIZE; });

			m_package_queue.push(std::move(package));
		}

		renderable_container& get_renderable_container() { return m_renderables; }
		light_renderable_container& get_light_renderable_container() { return m_light_renderables; }

	private:
		core();
		void loop();
		bool m_should_end;
		static core* m_instance;

		std::thread m_looping_thread;
		std::queue<std::unique_ptr<core_package>> m_package_queue;
		std::mutex m_package_queue_mutex;
		std::condition_variable m_package_queue_condvar;


		std::map<unique_id, uint32_t> m_renderable_type_table;
		std::map<unique_id, uint32_t> m_light_renderable_type_table;

		renderable_container m_renderables;
		light_renderable_container m_light_renderables;

		
		template<size_t... render_passes>
		void record_and_render(const std::optional<camera_data>& cam, std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_mask, 
			std::index_sequence<render_passes...>)
		{
			auto l = { (render_pass_typename<render_passes>::type::instance()->record_and_render(cam, record_mask), 0)... };
		}

		template<size_t... render_passes>
		void wait_for_finish(std::index_sequence<render_passes...>)
		{
			auto l = { (render_pass_typename<render_passes>::type::instance()->wait_for_finish(), 0)... };
		}
	};
}

