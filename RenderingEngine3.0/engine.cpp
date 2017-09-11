#include "engine.h"
#include "base.h"
#include "disassembler.h"

using namespace rcq;

engine* engine::m_instance = nullptr;


engine::engine()
{
	m_package.reset(new command_package);
	containers::init();

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
	basei = base::instance()->get_info();
	
}

engine::~engine()
{
	base::destroy();
	containers::destroy();
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
	disassembler::instance()->push_package(std::move(m_package));
	m_package.reset(new command_package);
}