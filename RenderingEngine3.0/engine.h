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

		void cmd_build_material(unique_id id, const material_data& mat_data,  const texfiles& texs, MAT_TYPE type)
		{
			m_command_p->resource_manager_p.build_mat.emplace_back(id, mat_data, texs, type);
		}

		void cmd_destroy_material(unique_id uid)
		{
			m_command_p->resource_manager_p.destroy_mat.push_back(uid);
		}

		void cmd_build_mesh(unique_id id, const std::string& filename, bool calc_tb)
		{
			m_command_p->resource_manager_p.build_mesh.emplace_back(id, filename, calc_tb);
		}

		void cmd_destroy_mesh(unique_id uid)
		{
			m_command_p->resource_manager_p.destroy_mesh.push_back(uid);
		}

		void cmd_build_transform(unique_id id, const transform_data& data, USAGE usage)
		{
			m_command_p->resource_manager_p.build_tr.emplace_back(id, data, usage);
		}

		void cmd_destroy_transform(unique_id id, USAGE usage)
		{
			m_command_p->resource_manager_p.destroy_tr.emplace_back(id, usage);
		}

		void cmd_build_renderable(unique_id id, unique_id tr_id, unique_id mesh_id, unique_id mat_id, LIFE_EXPECTANCY life_exp)
		{
			m_command_p->core_p.build_renderable.emplace_back(id, tr_id, mesh_id, mat_id, life_exp);
		}

		void cmd_destroy_renderable(unique_id uid)
		{
			m_command_p->core_p.destroy_renderable.push_back(uid);
		}

		void cmd_destroy_transform(unique_id id, USAGE usage)
		{
			m_resource_manager_p->destroy_tr.emplace_back(id, usage);
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