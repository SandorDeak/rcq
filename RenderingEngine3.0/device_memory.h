#pragma once

#include "vulkan.h"

namespace rcq
{
	class device_memory 
	{
	public:
		device_memory() {}

		device_memory(VkDeviceSize max_alignment, VkDevice device, const VkDeviceMemory* handle, device_memory* upstream) :
			m_device(device),
			m_handle(handle),
			m_max_alignment(max_alignment),
			m_upstream(upstream)
		{}

		virtual ~device_memory() {}

		VkDeviceMemory handle() const
		{
			return *m_handle;
		}

		const VkDeviceMemory* handle_ptr()
		{
			return m_handle;
		}

		VkDevice device() const
		{
			return m_device;
		}

		size_t map(VkDeviceSize p, VkDeviceSize size)
		{
			void* data;
			vkMapMemory(m_device, *m_handle, p, size, 0, &data);
			return reinterpret_cast<size_t>(data);
		}
		void unmap()
		{
			vkUnmapMemory(m_device, *m_handle);
		}

		virtual VkDeviceSize allocate(VkDeviceSize size, VkDeviceSize alignment) = 0;
		virtual void deallocate(VkDeviceSize p) = 0;

		static VkDeviceSize align(VkDeviceSize p, VkDeviceSize alignment)
		{
			return (p + (alignment - 1)) & (~(alignment - 1));
		}

		VkDeviceSize max_alignment()
		{
			return m_max_alignment;
		}

		device_memory* upstream() const
		{
			return m_upstream;
		}

	protected:

		void init(VkDeviceSize max_alignment, VkDevice device, const VkDeviceMemory* handle, device_memory* upstream)
		{
			m_device = device;
			m_handle = handle;
			m_max_alignment = max_alignment;
			m_upstream = upstream;
		}

		device_memory* m_upstream;
		VkDeviceSize m_max_alignment;
		const VkDeviceMemory* m_handle;
		VkDevice m_device;
	};
}