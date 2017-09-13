#pragma once
#include "foundation.h"

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
			m_package_queue.push(std::move(package));
		}

	private:
		core();
		void loop();
		bool m_should_end;
		static core* m_instance;

		std::thread m_looping_thread;
		std::queue<std::unique_ptr<core_package>> m_package_queue;
		std::mutex m_package_queue_mutex;

		std::map<unique_id, renderable&> m_renderable_refs;

		std::array<std::vector<renderable>, MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> m_renderables;
	};
}

