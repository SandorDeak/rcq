#pragma once

#include "vulkan_allocator.h"
#include "foundation.h"

namespace rcq
{
	class device_memory_resource : public memory_resource
	{
		device_memory_resource(VkDevice device, uint32_t memory_type_index, const vulkan_allocator* vk_alloc) :
			memory_resource(std::numeric_limits<VkDeviceSize>::max(), nullptr),
			m_vk_alloc(vk_alloc),
			m_device(device),
			m_alloc{}
		{
			m_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			m_alloc.memoryTypeIndex = memory_type_index;
		}

		uint64_t allocate(uint64_t size, uint64_t alignment=0) override
		{
			VkDeviceMemory mem;
			m_alloc.allocationSize = size;
			VkResult res = vkAllocateMemory(m_device, &m_alloc, *m_vk_alloc, &mem);
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
			default:
				return mem;
				break;
			}
		}

		void deallocate(uint64_t p) override
		{
			vkFreeMemory(m_device, p, *m_vk_alloc);
		}

	private:
		VkMemoryAllocateInfo m_alloc;
		VkDevice m_device;
		const vulkan_allocator* m_vk_alloc;
	};

}