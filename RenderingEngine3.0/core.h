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
			while (m_package_queue.size() >= CORE_COMMAND_QUEUE_MAX_SIZE); //blocking
			std::lock_guard<std::mutex> lock(m_package_queue_mutex);
			m_package_queue.push(std::move(package));
		}

		renderable_container& get_renderable_container() { return m_renderables; }

	private:
		core();
		void loop();
		bool m_should_end;
		static core* m_instance;

		std::thread m_looping_thread;
		std::queue<std::unique_ptr<core_package>> m_package_queue;
		std::mutex m_package_queue_mutex;

		std::map<unique_id, renderable&> m_renderable_refs;

		renderable_container m_renderables;
		
		template<size_t... render_passes>
		void record_and_render(const std::optional<camera_data>& cam, std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_mask, 
			std::index_sequence<render_passes...>)
		{
			auto l = { (render_pass_typename<render_passes>::type::instance()->record_and_render(cam, record_mask), 0)... };
		}
	};
}

