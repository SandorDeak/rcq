#pragma once

#include "foundation.h"

namespace rcq
{
	class resource_manager
	{
	public:
		resource_manager(const resource_manager&) = delete;
		resource_manager(resource_manager&&) = delete;

		~resource_manager();

		static void init(const base_info& info);
		static void destroy();
		static resource_manager* instance() { return m_instance; }

		texture create_depth_async(RENDER_PASS pass, int width, int height, int layer_count);
		void destroy_depth_async(RENDER_PASS);

		void process_package_async(resource_manager_package&& package);

	private:

		resource_manager(const base_info& info);
		void destroy_texture(const texture& tex);

		mesh build_mesh(std::string filename, bool calc_tb);
		material build_material(material_data data, texfiles files, MAT_TYPE type);
		transform build_transform(transform_data data, USAGE usage);

		void destroy_mesh(unique_id id);
		void destroy_material(unique_id id);
		void destroy_tr(unique_id id);


		texture create_depth(int width, int height, int layer_count);
		void destroy_depth(RENDER_PASS);

		void work();
		void do_tasks(resource_manager_task_package& package);

		static resource_manager* m_instance;

		std::thread m_working_thread;
		bool m_should_end;

		const base_info m_base;

		std::map<unique_id, mesh> m_meshes_ready;
		std::map<unique_id, std::future<mesh>> m_meshes_proc;

		std::map<unique_id, material> m_mats_ready;
		std::map<unique_id, std::future<material>> m_mats_proc;

		std::map<unique_id, transform> m_tr_ready;
		std::map<unique_id, std::future<transform>> m_tr_proc;

		std::map<RENDER_PASS, texture> m_depth_ready;
		std::map<RENDER_PASS, std::future<texture>> m_depth_proc;

		std::queue<std::unique_ptr<resource_manager_task_package>> m_task_p_queue;
		std::mutex m_task_p_queue_mutex;

	};

}