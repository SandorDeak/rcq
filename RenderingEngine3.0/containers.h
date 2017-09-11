#pragma once

#include "foundation.h"

namespace rcq
{

	class containers
	{
	public:
		
		containers(const containers&) = delete;
		containers(containers&&) = delete;
		~containers();

		static void init();
		static void destroy();
		static containers* instance() { return m_instance; }

		material_data* new_mat(USAGE usage);
		void delete_mat(material_data*, USAGE usage);

		transform_data* new_tr(USAGE usage);
		void delete_tr(transform_data*, USAGE usage);

	private:
		friend class engine;
		containers();

		static containers* m_instance;

		std::vector<material_data> m_pool_mat_dynamic;
		std::vector<material_data> m_pool_mat_static;
		std::vector<transform_data> m_pool_tr_dynamic;
		std::vector<transform_data> m_pool_tr_static;

		std::stack<size_t> m_available_mat_static;
		std::stack<size_t> m_available_mat_dynamic;
		std::stack<size_t> m_available_tr_static;
		std::stack<size_t> m_available_tr_dynamic;
	};

}