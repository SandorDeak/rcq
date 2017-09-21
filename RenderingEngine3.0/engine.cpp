#include "engine.h"
#include "base.h"
#include "disassembler.h"
#include "core.h"
#include "resource_manager.h"
#include "basic_pass.h"
#include "device_memory.h"

using namespace rcq;

engine* engine::m_instance = nullptr;


engine::engine()
{
	m_command_p.reset(new command_package);

	//init base
	base_create_info base_info = {};
	base_info.device_extensions=
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	base_info.enable_validation_layers = true;
	base_info.height = 768;
	base_info.width = 1024;
	base_info.instance_extensions=
	{
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};
	base_info.validation_layers=
	{
		"VK_LAYER_LUNARG_standard_validation"
	};
	base_info.window_name = "RCQ Engine";
	base_info.device_features.samplerAnisotropy = VK_TRUE;
	base_info.device_features.geometryShader = VK_TRUE;

	base::init(base_info);
	m_base = base::instance()->get_info();
	m_window_size.x = static_cast<float>(m_base.swap_chain_image_extent.width);
	m_window_size.y = static_cast<float>(m_base.swap_chain_image_extent.height);

	device_memory::init(m_base);
	resource_manager::init(m_base);
	core::init();
	basic_pass::init(m_base, core::instance()->get_renderable_container());

	disassembler::init();

}

engine::~engine()
{
	disassembler::destroy();
	core::destroy();
	basic_pass::destroy();
	resource_manager::destroy();
	device_memory::destroy();
	base::destroy();
}

void engine::init()
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("engie is already initialised!");
	}
	m_instance = new engine();
}

void engine::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy engine, it doesn't exist!");
	}

	delete m_instance;
}

void engine::cmd_dispatch()
{
	disassembler::instance()->push_package(std::move(m_command_p));
	m_command_p.reset(new command_package);
}