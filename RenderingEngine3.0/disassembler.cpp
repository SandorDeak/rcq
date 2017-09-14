#include "disassembler.h"
#include "resource_manager.h"
#include "core.h"

using namespace rcq;

disassembler* disassembler::m_instance = nullptr;


disassembler::disassembler()
{
	m_should_end = false;
	m_looping_thread = std::thread(std::bind(&disassembler::loop, this));
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
				std::lock_guard<std::mutex> lock(m_command_queue_mutex);
				package = std::move(m_command_queue.front());
				m_command_queue.pop();
			}
			//resource_manager::instance()->process_package_async(std::move(package->resource_manager_p));
			if (package->resource_manager_build_p)
				resource_manager::instance()->process_build_package(std::move(package->resource_manager_build_p.value()));
			if (package->core_p && package->resource_mananger_destroy_p)
			{
				package->core_p.value().confirm_destroy = {};
				package->resource_mananger_destroy_p.value().destroy_confirmation = package->core_p.value().confirm_destroy->get_future();
				
				core::instance()->push_package(std::make_unique<core_package>(std::move(package->core_p.value())));
				
				resource_manager::instance()->push_destroy_package(
					std::make_unique<destroy_package>(std::move(package->resource_mananger_destroy_p.value())));
			}
			else if (package->core_p)
				core::instance()->push_package(std::make_unique<core_package>(std::move(package->core_p.value())));
			else if (package->resource_mananger_destroy_p)
				resource_manager::instance()->push_destroy_package(
					std::make_unique<destroy_package>(std::move(package->resource_mananger_destroy_p.value())));
		}
	}
}
