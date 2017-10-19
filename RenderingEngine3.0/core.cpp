#include "core.h"

#include "resource_manager.h"
#include "gta5_pass.h"

rcq::core* rcq::core::m_instance = nullptr;

rcq::core::core()
{
	m_should_end = false;
	m_looping_thread = std::thread([this]()
	{
		try
		{ 
			loop();
		}
		catch (const std::runtime_error& e)
		{
			std::cerr << e.what() << std::endl;
		}
	});
}


rcq::core::~core()
{
	m_should_end = true;
	m_looping_thread.join();
}

void rcq::core::init()
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("core is already initialised!");
	}
	m_instance = new core();
}

void rcq::core::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy core, it doesn't exist!");
	}

	delete m_instance;
}

void rcq::core::loop()
{
	constexpr size_t renderables_size = RENDERABLE_TYPE_COUNT;

	std::bitset<renderables_size> record_mask;
	record_mask.reset();

	while (!m_should_end || m_package)
	{
		if (m_package)
		{
			std::unique_ptr<core_package> package;
			{
				std::unique_lock<std::mutex> lock(m_package_mutex);
				package = std::move(m_package);
				m_package_condvar.notify_one();
			}
			
			//destroy renderables
			std::bitset<RENDERABLE_TYPE_COUNT> remove_deleted_renderables(false);

			for (uint32_t i = 0; i < renderables_size; ++i)
			{
				if (!package->destroy_renderable[i].empty())
				{
					for (auto id : package->destroy_renderable[i])
					{
						bool found = false;
						for (auto& r : m_renderables[i])
						{
							if (r.id == id)
							{
								r.destroy = true;
								found = true;
								break;
							}
						}
						if (!found)
							throw std::runtime_error("cannot delete renderable with the given id, it doesn't exist!");
					}
					m_renderables[i].erase(std::remove_if(m_renderables[i].begin(), m_renderables[i].end(),
						[](renderable& r) {return r.destroy; }), m_renderables[i].end());
					record_mask.set(i);
				}
			}
				

			//build renderables
			build_renderables(package->build_renderable, std::make_index_sequence<renderables_size>());
			for (uint32_t i = 0; i < renderables_size; ++i)
			{
				if (!package->build_renderable[i].empty())
					record_mask.set(i);
			}

			//update tr
			if (!package->update_tr.empty())
			{
				//wait_for_finish(std::make_index_sequence<RENDER_PASS_COUNT>());
				//basic_pass::instance()->wait_for_finish();
				resource_manager::instance()->update_tr(package->update_tr);
			}

			if (package->render)
			{
				/*record_and_render(package->view, package->proj_info, record_mask);
				record_mask.reset();*/
				m_render_settings = package->settings;
				gta5_pass::instance()->render(m_render_settings, record_mask);
				record_mask.reset();
			}

			if (package->confirm_destroy)
			{
				//wait_for_finish(std::make_index_sequence<RENDER_PASS_COUNT>());
				//basic_pass::instance()->wait_for_finish();
				package->confirm_destroy->set_value();
			}
		}
	}

}

/*void rcq::core::record_and_render(const glm::mat4& view, const std::optional<update_proj>& proj_info,
	std::bitset<RENDERABLE_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_mask)
{
	auto shadow_map_s = omni_light_shadow_pass::instance()->record_and_render(view, proj_info, record_mask, m_begin_s);
	m_begin_s=basic_pass::instance()->record_and_render(view, proj_info, record_mask, shadow_map_s);
}*/

template<size_t rend_type>
void rcq::core::build_renderables_impl(const std::vector<build_renderable_info>& build_infos)
{
	for (auto& info : build_infos)
	{
		renderable r;
		r.id = std::get<BUILD_RENDERABLE_INFO_RENDERABLE_ID>(info);
		r.destroy = false;
		auto res = resource_manager::instance()->get_res<rend_type>(
			std::get<BUILD_RENDERABLE_INFO_MAT_OR_LIGHT_ID>(info));

		r.mat_light_ds = res.ds;

		if constexpr (rend_type == RENDERABLE_TYPE_LIGHT_OMNI)
		{
			r.shadow_map = res.shadow_map;
			r.shadow_map_fb = res.shadow_map_fb;
		}

		if constexpr (rend_type  == RENDERABLE_TYPE_MAT_OPAQUE)
		{
			r.m = resource_manager::instance()->get_res<RESOURCE_TYPE_MESH>(
				std::get<BUILD_RENDERABLE_INFO_MESH_ID>(info));
			r.tr_ds = resource_manager::instance()->get_res<RESOURCE_TYPE_TR>(
				std::get<BUILD_RENDERABLE_INFO_TR_ID>(info)).ds;
		}

		m_renderables[rend_type].push_back(r);
	}
}