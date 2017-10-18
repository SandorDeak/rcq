#include "engine.h"
#include "base.h"
#include "core.h"
#include "resource_manager.h"
//#include "basic_pass.h"
//#include "omni_light_shadow_pass.h"
#include "gta5_pass.h"
#include "device_memory.h"

using namespace rcq;

engine* engine::m_instance = nullptr;


engine::engine()
{
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
	base_info.device_features.depthBounds = VK_TRUE;

	base::init(base_info);
	m_base = base::instance()->get_info();
	m_window_size.x = static_cast<float>(m_base.swap_chain_image_extent.width);
	m_window_size.y = static_cast<float>(m_base.swap_chain_image_extent.height);

	device_memory::init(m_base);
	resource_manager::init(m_base);
	core::init();
	gta5_pass::init(m_base, core::instance()->get_renderable_container());
	/*basic_pass::init(m_base, core::instance()->get_renderable_container());
	omni_light_shadow_pass::init(m_base, core::instance()->get_renderable_container());*/
}

engine::~engine()
{
	core::destroy();
	gta5_pass::destroy();
	//basic_pass::destroy();
	//omni_light_shadow_pass::destroy();
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
	if (m_build_p)
	{
		resource_manager::instance()->process_build_package(std::move(*m_build_p.get()));
		m_build_p.release();
	}
	if (m_destroy_p)
	{
		if (!m_core_p)
			m_core_p.reset(new core_package);
		m_core_p->confirm_destroy.emplace();
		m_destroy_p->destroy_confirmation.emplace(m_core_p->confirm_destroy->get_future());

		resource_manager::instance()->push_destroy_package(std::move(m_destroy_p));
	}
	if (m_core_p)
		core::instance()->push_package(std::move(m_core_p));
}