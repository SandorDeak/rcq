#include "disassembler.h"
#include "resource_manager.h"

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
			//disassemble package
			for (const auto& build_mat : package->build_mat)
				resource_manager::instance()->build_material(build_mat);
			for (const auto& build_mesh : package->build_mesh)
				resource_manager::instance()->build_mesh(build_mesh);
			for (const auto& build_tr : package->build_tr)
				resource_manager::instance()->build_transform(build_tr);
			for (const auto& destroy_mat : package->destroy_mat)
				resource_manager::instance()->destroy_material(destroy_mat);
			for (const auto& destroy_mesh : package->destroy_mesh)
				resource_manager::instance()->destroy_mesh(destroy_mesh);
			for (const auto& destroy_tr : package->destroy_tr)
				resource_manager::instance()->destroy_tr(destroy_tr);

			auto to_core=std::make_unique<core_package>(
			
				std::move(package->build_renderable),
				std::move(package->destroy_renderable),
				std::move(package->update_tr),
				std::move(package->update_camera)
			);

			//send to core
			
		}
	}
}
