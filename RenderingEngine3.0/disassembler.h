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
			std::unique_lock<std::mutex> lock(m_command_queue_mutex);
			if (m_command_queue.size() >= DISASSEMBLER_COMMAND_QUEUE_MAX_SIZE)
				m_command_queue_condvar.wait(lock, [this]() {return m_command_queue.size() < DISASSEMBLER_COMMAND_QUEUE_MAX_SIZE; }); //blocking

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
		std::condition_variable m_command_queue_condvar;

	};

}

