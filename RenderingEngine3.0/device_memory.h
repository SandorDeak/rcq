#pragma once
#include "foundation.h"
#include "freelist_resource.h"
#include "vk_memory_resource.h"
#include "vk_allocator.h"

#include "vector.h"

namespace rcq
{
	class device_memory
	{
	public:
		enum
		{
			local,
			mappable,
			count
		};

		device_memory(const device_memory&) = delete;
		device_memory(device_memory&&) = delete;
		~device_memory();

		static void init(const base_info& base, memory_resource* host_memory_resource);
		static void destroy();
		static device_memory* instance() { return m_instance; }

		/*void acquire_resource()
		{
			bool exp=true;
			while (!m_memory_res_available.compare_exchange_weak(exp, false, std::memory_order_acquire))
				exp = true;
		}

		void release_resource()
		{
			bool exp = false;
			if (!m_memory_res_available.compare_exchange_weak(exp, true, std::memory_order_release))
				throw std::runtime_error("synchronization error at device memory!");
		}*/

		freelist_resource* resource()
		{
			return m_memory_res;
		}

		uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);

	private:
		freelist_resource* m_memory_res;
		vk_memory_resource* m_vk_memory_res;

		device_memory(const base_info& base, memory_resource* host_memory_resource);

		static device_memory* m_instance;

		const base_info& m_base;
		   
		static memory_resource*  m_host_memory_resource;

		vk_allocator m_vk_alloc;

		uint32_t m_mappable_memory_type_index;

	};
}