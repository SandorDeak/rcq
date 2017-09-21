#include "core.h"

#include "resource_manager.h"

using namespace rcq;

core* core::m_instance = nullptr;

core::core()
{
	m_should_end = false;
	m_looping_thread = std::thread([this]()
	{
		/*try
		{*/
			loop();
		/*}
		catch (const std::runtime_error& e)
		{
			std::cerr << e.what() << std::endl;
		}*/
	});
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
	std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_mask;
	record_mask.reset();

	while (!m_should_end || !m_package_queue.empty())
	{
		if (!m_package_queue.empty())
		{
			std::unique_ptr<core_package> package;
			{
				std::unique_lock<std::mutex> lock(m_package_queue_mutex);
				package = std::move(m_package_queue.front());
				m_package_queue.pop();
				if (m_package_queue.size() == CORE_COMMAND_QUEUE_MAX_SIZE - 1)
					m_package_queue_condvar.notify_one();
			}
		
			std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> remove_deleted_renderables(false);

			for (auto& destroy_id : package->destroy_renderable)
			{
				auto it = m_renderable_type_table.find(destroy_id);
				if (it == m_renderable_type_table.end())
					throw std::runtime_error("cannot delete renderable with the given id, it doesn't exist!");
				for (auto& r : m_renderables[it->second])
				{
					if (r.id == destroy_id)
						r.destroy = true;
				}
				remove_deleted_renderables.set(it->second);
				record_mask.set(it->second);
				m_renderable_type_table.erase(it);
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
					_mesh.vb,
					_mesh.veb,
					_mesh.ib,
					_mesh.size,
					_tr.ds,
					_mat.ds,
					type,
					false,
					std::get<BUILD_RENDERABLE_INFO_RENDERABLE_ID>(build_renderable)
				};
				record_mask.set(type);
				
				m_renderables[type].push_back(r);

				if (!m_renderable_type_table.insert_or_assign(std::get<BUILD_RENDERABLE_INFO_RENDERABLE_ID>(build_renderable),
					r.type).second)
				{
					throw std::runtime_error("id conflict!");
				}


			}

			if (!package->update_tr.empty())
			{
				wait_for_finish(std::make_index_sequence<RENDER_PASS_COUNT>());
				resource_manager::instance()->update_tr(package->update_tr);
			}

			if (package->render)
			{
				record_and_render(package->update_cam, record_mask, std::make_index_sequence<RENDER_PASS_COUNT>());
				record_mask.reset();
			}

			if (package->confirm_destroy)
			{
				wait_for_finish(std::make_index_sequence<RENDER_PASS_COUNT>());
				package->confirm_destroy->set_value();
			}
		}
	}
	
}
