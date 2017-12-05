#pragma once

#include "vk_allocator.h"
#include "device_memory_resource.h"
#include "foundation.h"

namespace rcq
{
	class vk_memory_resource : public device_memory_resource
	{
	public:
		vk_memory_resource(VkDevice device, uint32_t memory_type_index, const vk_allocator* vk_alloc) :
			device_memory_resource(std::numeric_limits<VkDeviceSize>::max(), device, VK_NULL_HANDLE, nullptr),
			m_vk_alloc(vk_alloc),
			m_memory_type_index(memory_type_index)
		{}

		~vk_memory_resource()
		{
			assert(m_handle == VK_NULL_HANDLE);
		}

		uint64_t allocate(uint64_t size, uint64_t alignment = 0) override
		{
			if (m_handle != VK_NULL_HANDLE)
				throw std::runtime_error("only one allocation possible at a time from a device memory resource object!");

			VkMemoryAllocateInfo alloc = {};
			alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc.memoryTypeIndex = m_memory_type_index;
			alloc.allocationSize = size;

			VkResult res = vkAllocateMemory(m_device, &alloc, *m_vk_alloc, &m_handle);
			switch (res)
			{
			case VK_ERROR_OUT_OF_HOST_MEMORY:
				throw std::runtime_error("vulkan memory allocation failed, out of host memory!");
				break;
			case VK_ERROR_OUT_OF_DEVICE_MEMORY:
				throw std::runtime_error("vulkan allocation failed, out of device memory!");
				break;
			case VK_ERROR_TOO_MANY_OBJECTS:
				throw std::runtime_error("vulkan allocation failed, too many objects!");
				break;
			case VK_SUCCESS:
				return 0;
			}
			
		}

		void deallocate(uint64_t p = 0) override
		{
			vkFreeMemory(m_device, m_handle, *m_vk_alloc);
			m_handle = VK_NULL_HANDLE;
		}

	private:
		uint32_t m_memory_type_index;
		const vk_allocator* m_vk_alloc;
	};

}