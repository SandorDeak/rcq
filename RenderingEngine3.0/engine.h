#pragma once

#include "foundation.h"
#include "containers.h"

namespace rcq
{
	struct command_package
	{
		
		std::vector<build_mat_info> build_mat_s;
		std::vector<unique_id> destroy_mat_s;
		std::vector<build_mesh_info> build_mesh;
		std::vector<unique_id> destroy_mesh;
		std::vector<build_renderable_info> build_renderable;
		std::vector<unique_id> destroy_renderable;
		std::pair<std::vector<char>, bool> copy_pool_tr_dynamic;
		std::pair<camera_data, bool> update_camera;
		bool render;

		command_package()
		{
			copy_pool_tr_dynamic.second = false;
			copy_pool_tr_dynamic.first.resize(containers::POOL_TR_DYNAMIC_SIZE);
			update_camera.second = false;
			render = false;
		}

		void reset()
		{
			build_mat_s.clear();
			destroy_mat_s.clear();
			build_mesh.clear();
			destroy_mesh.clear();
			build_renderable.clear();
			destroy_renderable.clear();
			copy_pool_tr_dynamic.second = false;
			update_camera.second = false;
		}
	};

	class engine
	{
	public:
		engine(const engine&) = delete;
		engine(engine&&) = delete;
		~engine();

		static void init();
		static void destroy();
		static engine* instance() { return m_instance; }

		GLFWwindow* get_window() { return basei.window; }

		void cmd_build_material_static(const build_mat_info& info)
		{
			m_package.build_mat_s.push_back(info);
		}

		void cmd_destroy_material_static(unique_id uid)
		{
			m_package.destroy_mat_s.push_back(uid);
		}

		void cmd_build_mesh(const build_mesh_info& info)
		{
			m_package.build_mesh.push_back(info);
		}

		void cmd_destroy_mesh(unique_id uid)
		{
			m_package.destroy_mesh.push_back(uid);
		}

		void cmd_build_renderable(const build_renderable_info& info)
		{
			m_package.build_renderable.push_back(info);
		}

		void cmd_destroy_renderable(unique_id uid)
		{
			m_package.destroy_renderable.push_back(uid);
		}

		void cmd_copy_pool_tr_dynamic()
		{
			memcpy(m_package.copy_pool_tr_dynamic.first.data(), 
				containers::instance()->m_pool_tr_dynamic.data(), containers::POOL_TR_DYNAMIC_SIZE);
			m_package.copy_pool_tr_dynamic.second = true;
		}

		void cmd_update_camera(const camera_data* cam)
		{
			m_package.update_camera.first = *cam;
			m_package.update_camera.second = true;
		}

		void cmd_render()
		{
			m_package.render = true;
		}

		void cmd_dispatch()
		{
			//send package further
			m_package.reset();
		}

	private:
		engine();
		static engine* m_instance;

		command_package m_package;

		base_info basei;
	};

}