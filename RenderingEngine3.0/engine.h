#pragma once

#include "foundation.h"

namespace rcq
{

	class engine
	{
	public:
		engine(const engine&) = delete;
		engine(engine&&) = delete;
		~engine();

		static void init();
		static void destroy();
		static engine* instance() { return m_instance; }

		GLFWwindow* get_window() { return m_base.window; }

		template<RESOURCE_TYPE type, typename... Ts>
		void cmd_build(Ts&&... args)
		{
			std::get<type>(m_command_p->resource_manager_p.build_p).emplace_back(std::forward<Ts>(args)...);
		}

		template<RESOURCE_TYPE type>
		void cmd_destroy(unique_id id)
		{
			std::get<type>(m_command_p->resource_manager_p.destroy_p).push_back(id);
		}

		void cmd_build_renderable(unique_id id, unique_id tr_id, unique_id mesh_id, unique_id mat_id, LIFE_EXPECTANCY life_exp)
		{
			m_command_p->core_p.build_renderable.emplace_back(id, tr_id, mesh_id, mat_id, life_exp);
		}

		void cmd_destroy_renderable(unique_id uid)
		{
			m_command_p->core_p.destroy_renderable.push_back(uid);
		}

		void cmd_update_camera(const camera_data& cam)
		{
			m_command_p->core_p.update_cam = cam;
		}

		void cmd_update_transform(unique_id id, const transform_data& tr_data)
		{
			m_command_p->core_p.update_tr.emplace_back(id, tr_data);
		}

		void cmd_render()
		{
			m_command_p->core_p.render = true;
		}

		void cmd_dispatch();

	private:
		engine();
		static engine* m_instance;

		std::unique_ptr<command_package> m_command_p;

		base_info m_base;
	};
}