#pragma once

#include "vk_allocator.h"
#include "device_memory.h"
#include "foundation2.h"

namespace rcq
{
	class vk_memory : public device_memory
	{
	public:
		vk_memory() {}

		vk_memory(VkDevice device, uint32_t memory_type_index, const vk_allocator* vk_alloc) :
			device_memory(std::numeric_limits<VkDeviceSize>::max(), device, nullptr, nullptr),
			m_vk_alloc(vk_alloc),
			m_memory_type_index(memory_type_index)
		{
			m_handle = &m_real_handle;
		}

		void init(VkDevice device, uint32_t memory_type_index, const vk_allocator* vk_alloc)
		{
			device_memory::init(std::numeric_limits<VkDeviceSize>::max(), device, VK_NULL_HANDLE, nullptr);

			m_vk_alloc = vk_alloc;
			m_memory_type_index = memory_type_index;
		}

		~vk_memory()
		{
			assert(m_real_handle == VK_NULL_HANDLE);
		}

		VkDeviceSize allocate(VkDeviceSize size, VkDeviceSize alignment = 0) override
		{
			assert(m_real_handle == VK_NULL_HANDLE);

			VkMemoryAllocateInfo alloc = {};
			alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc.memoryTypeIndex = m_memory_type_index;
			alloc.allocationSize = size;

			assert(vkAllocateMemory(m_device, &alloc, *m_vk_alloc, &m_real_handle)==VK_SUCCESS);

			return 0;
		}

		void deallocate(VkDeviceSize p) override
		{
			assert(p == 0 && m_real_handle!=VK_NULL_HANDLE);
			vkFreeMemory(m_device, m_real_handle, *m_vk_alloc);
			m_real_handle = VK_NULL_HANDLE;
		}

	private:
		VkDeviceMemory m_real_handle;
		uint32_t m_memory_type_index;
		const vk_allocator* m_vk_alloc;
	};

}