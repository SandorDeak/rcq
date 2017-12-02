#pragma once
#include "foundation.h"
#include "freelist_resource.h"
#include "device_memory_resource.h"

#include "vector.h"

namespace rcq
{
	class device_memory
	{
	public:
		device_memory(const device_memory&) = delete;
		device_memory(device_memory&&) = delete;
		~device_memory();

		static void init(const base_info& base, memory_resource* host_memory_resource, uint64_t place);
		static void destroy();
		static device_memory* instance() { return m_instance; }

	private:

		VkPhysicalDeviceMemoryProperties m_memory_properties;

		
		vector<device_memory_resource> m_device_memory_resources;

		device_memory(const base_info& base, memory_resource* host_memory_resource); 

		static device_memory* m_instance;

		const base_info& m_base;

		memory_resource*  m_host_memory_resource;
	};
}