#include "core.h"

#include "resource_manager.h"

using namespace rcq;

core* core::m_instance = nullptr;

core::core()
{
	m_should_end = false;
	m_looping_thread = std::thread(std::bind(&core::loop, this));
}


core::~core()
{
	m_should_end = true;
	m_looping_thread.join();
}

void core::init()
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("core is already initialised!");
	}
	m_instance = new core();
}

void core::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy core, it doesn't exist!");
	}

	delete m_instance;
}

void core::loop()
{
	while (!m_should_end || !m_package_queue.empty())
	{
		if (!m_package_queue.empty())
		{
			std::unique_ptr<core_package> package;
			{
				std::lock_guard<std::mutex> lock(m_package_queue_mutex);
				package = std::move(m_package_queue.front());
				m_package_queue.pop();
			}

			if (package->confirm_destroy)
				package->confirm_destroy->set_value();

			std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> rerecord_command_buffers(false);
			std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> remove_deleted_renderables(false);

			for (auto& destroy_id : package->destroy_renderable)
			{
				auto it = m_renderable_refs.find(destroy_id);
				if (it == m_renderable_refs.end())
					throw std::runtime_error("cannot delete renderable with the given id, it doesn't exist!");
				it->second.destroy = true;
				remove_deleted_renderables.set(it->second.type);
				rerecord_command_buffers.set(it->second.type);
				m_renderable_refs.erase(it);
			}

			for (size_t i = 0; i < m_renderables.size(); ++i)
			{
				if (remove_deleted_renderables[i])
				{
					m_renderables[i].erase(std::remove_if(m_renderables[i].begin(), m_renderables[i].end(),
						[](const renderable& r) {return r.destroy; }), m_renderables[i].end());
				}
			}

			for (auto& build_renderable : package->build_renderable)
			{
				material _mat = resource_manager::instance()->get<RESOURCE_TYPE_MAT>(
					std::get<BUILD_RENDERABLE_INFO_MAT_ID>(build_renderable));
				mesh _mesh = resource_manager::instance()->get<RESOURCE_TYPE_MESH>(
					std::get<BUILD_RENDERABLE_INFO_MESH_ID>(build_renderable));
				transform _tr = resource_manager::instance()->get<RESOURCE_TYPE_TR>(
					std::get<BUILD_RENDERABLE_INFO_TR_ID>(build_renderable));
				uint32_t type = _mat.type*std::get<BUILD_RENDERABLE_INFO_LIFE_EXPECTANCY>(build_renderable);

				renderable r = {
					_mat.ds,
					_mesh.vb,
					_mesh.veb,
					_mesh.ib,
					_mesh.size,
					_tr.ds,
					type,
					false
				};
				rerecord_command_buffers.set(type);
				

				if (!m_renderable_refs.insert_or_assign(std::get<BUILD_RENDERABLE_INFO_RENDERABLE_ID>(build_renderable),
					m_renderables[type].emplace_back(std::move(r))).second)
				{
					throw std::runtime_error("id conflict!");
				}


			}

			
		}
	}
}
