#pragma once

#include "foundation.h"
#include "containers.h"

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

		GLFWwindow* get_window() { return basei.window; }

		void cmd_build_material_static(const build_mat_info& info)
		{
			m_package->build_mat_s.push_back(info);
		}

		void cmd_destroy_material_static(unique_id uid)
		{
			m_package->destroy_mat_s.push_back(uid);
		}

		void cmd_build_mesh(const build_mesh_info& info)
		{
			m_package->build_mesh.push_back(info);
		}

		void cmd_destroy_mesh(unique_id uid)
		{
			m_package->destroy_mesh.push_back(uid);
		}

		void cmd_build_renderable(const build_renderable_info& info)
		{
			m_package->build_renderable.push_back(info);
		}

		void cmd_destroy_renderable(unique_id uid)
		{
			m_package->destroy_renderable.push_back(uid);
		}

		void cmd_copy_pool_tr_dynamic()
		{
			memcpy(m_package->copy_pool_tr_dynamic.first.data(), 
				containers::instance()->m_pool_tr_dynamic.data(), POOL_TR_DYNAMIC_SIZE);
			m_package->copy_pool_tr_dynamic.second = true;
		}

		void cmd_update_camera(const camera_data* cam)
		{
			m_package->update_camera.first = *cam;
			m_package->update_camera.second = true;
		}

		void cmd_render()
		{
			m_package->render = true;
		}

		void cmd_dispatch();

	private:
		engine();
		static engine* m_instance;

		std::unique_ptr<command_package> m_package;

		base_info basei;
	};

}