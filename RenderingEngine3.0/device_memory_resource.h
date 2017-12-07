#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>

#include "memory_resource.h"

namespace rcq
{
	class device_memory_resource : public memory_resource
	{
	public:
		device_memory_resource() {}

		device_memory_resource(uint64_t max_alignment, VkDevice device, VkDeviceMemory handle, device_memory_resource* upstream) :
			memory_resource(max_alignment, upstream),
			m_device(device),
			m_handle(handle)
		{}

		virtual ~device_memory_resource() {}

		VkDeviceMemory handle() const
		{
			return m_handle;
		}

		VkDevice device() const
		{
			return m_device;
		}

		uint64_t map(uint64_t p, uint64_t size)
		{
			void* data;
			vkMapMemory(m_device, m_handle, static_cast<VkDeviceSize>(p), static_cast<VkDeviceSize>(size), 0, &data);
			return reinterpret_cast<uint64_t>(data);
		}
		void unmap()
		{
			vkUnmapMemory(m_device, m_handle);
		}

	protected:

		void init(uint64_t max_alignment, VkDevice device, VkDeviceMemory handle, device_memory_resource* upstream)
		{
			memory_resource::init(max_alignment, upstream);
			m_device = device;
			m_handle = handle;
		}

		//device_memory_resource* m_upstream;
		uint64_t m_max_alignment;
		VkDeviceMemory m_handle;
		VkDevice m_device;
	};
}