#pragma once

#include "memory_resource.h"
#include "malloc_free_resource.h"
#include "freelist_resource_host.h"
#include "freelist_resource.h"

namespace rcq
{
	class host_memory
	{
	public:
		host_memory(const host_memory&) = delete;
		host_memory(host_memory&&) = delete;
		~host_memory();

		static void init();
		static void destroy();
		static host_memory* instance() { return m_instance; }

		freelist_resource* resource() { return m_base_resource; }
		malloc_free_resource* os_resource() { return &m_os_resource; }


	private:
		host_memory();

		static host_memory* m_instance;

		malloc_free_resource m_os_resource;
		//freelist_resource_host* m_base_resource;
		freelist_resource<size_t>* m_base_resource;
	};
}

