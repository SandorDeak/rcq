#include "disassembler.h"
#include "resource_manager.h"
#include "core.h"

using namespace rcq;

disassembler* disassembler::m_instance = nullptr;


disassembler::disassembler()
{
	m_should_end = false;
	m_looping_thread = std::thread([this]()
	{
		loop();
	});
}


disassembler::~disassembler()
{
	m_should_end = true;
	m_looping_thread.join();
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
	while (!m_should_end || !m_command_queue.empty())
	{
		if (!m_command_queue.empty())
		{
			std::unique_ptr<command_package> package;
			{
				std::unique_lock<std::mutex> lock(m_command_queue_mutex);
				package = std::move(m_command_queue.front());
				m_command_queue.pop();
				if (m_command_queue.size() == DISASSEMBLER_COMMAND_QUEUE_MAX_SIZE - 1)
					m_command_queue_condvar.notify_one();
			}

			if (package->resource_manager_build_p)
				resource_manager::instance()->process_build_package(std::move(package->resource_manager_build_p.value()));
			if (package->resource_mananger_destroy_p)
			{
				if (!package->core_p)
					package->core_p.emplace();
				package->core_p.value().confirm_destroy.emplace();
				package->resource_mananger_destroy_p.value().destroy_confirmation.emplace(
					package->core_p.value().confirm_destroy->get_future());
				
				resource_manager::instance()->push_destroy_package(
					std::make_unique<destroy_package>(std::move(package->resource_mananger_destroy_p.value())));
			}
			if (package->core_p)
				core::instance()->push_package(std::make_unique<core_package>(std::move(package->core_p.value())));
		}
	}
}
