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
		glm::vec2 get_window_size() { return m_window_size; }

		template<RESOURCE_TYPE type, typename... Ts>
		void cmd_build(Ts&&... args)
		{
			if (!m_command_p->resource_manager_build_p)
				m_command_p->resource_manager_build_p.emplace();

			std::get<type>(m_command_p->resource_manager_build_p.value()).emplace_back(std::forward<Ts>(args)...);
		}

		template<RESOURCE_TYPE type>
		void cmd_destroy(unique_id id)
		{
			if (!m_command_p->resource_mananger_destroy_p)
				m_command_p->resource_mananger_destroy_p.emplace();

			std::get<type>(m_command_p->resource_mananger_destroy_p.value().ids).push_back(id);
		}

		void cmd_build_renderable(unique_id id, unique_id tr_id, unique_id mesh_id, unique_id mat_id, LIFE_EXPECTANCY life_exp)
		{
			if (!m_command_p->core_p)
				m_command_p->core_p.emplace();
			m_command_p->core_p.value().build_renderable.emplace_back(id, tr_id, mesh_id, mat_id, life_exp);
		}

		void cmd_build_light_renderable(unique_id id, unique_id res_id, LIFE_EXPECTANCY life_exp)
		{
			if (!m_command_p->core_p)
				m_command_p->core_p.emplace();
			m_command_p->core_p.value().build_light_renderable.emplace_back(id, res_id, life_exp);
		}

		void cmd_destroy_renderable(unique_id uid)
		{
			if (!m_command_p->core_p)
				m_command_p->core_p.emplace();
			m_command_p->core_p.value().destroy_renderable.push_back(uid);
		}

		void cmd_destroy_light_renderable(unique_id id)
		{
			if (!m_command_p->core_p)
				m_command_p->core_p.emplace();
			m_command_p->core_p.value().destroy_light_renderable.push_back(id);
		}

		void cmd_update_camera(const camera_data& cam)
		{
			if (!m_command_p->core_p)
				m_command_p->core_p.emplace();
			m_command_p->core_p.value().update_cam = cam;
		}

		void cmd_update_transform(unique_id id, const transform_data& tr_data)
		{
			if (!m_command_p->core_p)
				m_command_p->core_p.emplace();
			m_command_p->core_p.value().update_tr.emplace_back(id, tr_data);
		}

		void cmd_render()
		{
			if (!m_command_p->core_p)
				m_command_p->core_p.emplace();
			m_command_p->core_p.value().render = true;
		}

		void cmd_dispatch();

	private:
		engine();
		static engine* m_instance;
		glm::vec2 m_window_size;

		std::unique_ptr<command_package> m_command_p;

		base_info m_base;
	};
}