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

		static void init();
		static void destroy();
		static resource_manager* instance() { return m_instance; }

		void build_mesh(const build_mesh_info& info);
		void build_material(const build_mat_info& info);
		void build_transform(const build_tr_info& info);

		void destroy_mesh(unique_id id);
		void destroy_material(unique_id id);
		void destroy_tr(const std::pair<unique_id, USAGE>& info);
		

	private:
		typedef size_t tr_offset;

		resource_manager();

		mesh build_mesh_async(std::string filename, bool calc_tb);
		material build_material_async(material_data data, texfiles files);
		tr_offset build_tr_async(transform_data data, USAGE usage);

		static resource_manager* m_instance;

		std::map<unique_id, mesh> m_meshes_ready;
		std::map<unique_id, std::future<mesh>> m_meshes_proc;

		std::map<unique_id, material> m_mats_ready;
		std::map<unique_id, std::future<material>> m_mats_proc;

		std::map<unique_id, tr_offset> m_tr_static_ready;
		std::map<unique_id, std::future<tr_offset>> m_tr_static_proc;
		
		std::map<unique_id, tr_offset> m_tr_dynamic_ready;
		std::map<unique_id, std::future<tr_offset>> m_tr_dynamic_proc;
	};

}