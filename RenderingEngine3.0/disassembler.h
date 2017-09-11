#pragma once
#include "foundation.h"

namespace rcq
{
	class disassembler
	{
	public:
		~disassembler();
		disassembler(const disassembler&) = delete;
		disassembler(disassembler&&) = delete;

		static void init();
		static void destroy();
		static disassembler* instance() { return m_instance; }

		void push_package(std::unique_ptr<command_package>&& package)
		{
			while (m_command_queue.size() >= DISASSEMBLER_COMMAND_QUEUE_MAX_SIZE); //blocking

			std::lock_guard<std::mutex> lock(m_command_queue_mutex);
			m_command_queue.push(std::move(package));
		}

	private:

		disassembler();
		void loop();

		std::thread m_looping_thread;

		bool m_should_end;
		static disassembler* m_instance;

		std::queue<std::unique_ptr<command_package>> m_command_queue;
		std::mutex m_command_queue_mutex;

	};

}

