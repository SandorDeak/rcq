#include "disassembler.h"

using namespace rcq;

disassembler* disassembler::m_instance = nullptr;


disassembler::disassembler()
{
	m_should_end = false;
	m_looping_thread = std::thread(std::bind(&disassembler::loop, this));
}


disassembler::~disassembler()
{
}

void disassembler::init()
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("engie is already initialised!");
	}
	m_instance = new disassembler();
}

void disassembler::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy engine, it doesn't exist!");
	}

	delete m_instance;
}

void disassembler::loop()
{
	while (!m_should_end)
	{
		if (!m_command_queue.empty())
		{
			std::unique_ptr<command_package> package;
			{
				std::lock_guard<std::mutex> lock(m_command_queue_mutex);
				package = std::move(m_command_queue.front());
				m_command_queue.pop();
			}

			
		}
	}
}
