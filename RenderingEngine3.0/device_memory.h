#pragma once
#include "foundation.h"

namespace rcq
{

	class device_memory
	{
	public:
		device_memory(const device_memory&) = delete;
		device_memory(device_memory&&) = delete;
		~device_memory();

		static void init(const base_info& base);
		static void destroy();
		static device_memory* instance() { return m_instance; }

		/*cell_info alloc_buffer_memory(USAGE usage, VkBuffer buffer, void** mapped_data);

		void free_buffer(USAGE usage, const cell_info& cell);
		size_t get_cell_size(USAGE usage) { return m_cell_size[usage]; }*/

		VkDeviceMemory allocate(VkBuffer b, VkMemoryPropertyFlags properties)
		{
			VkMemoryRequirements mr;
			vkGetBufferMemoryRequirements(m_base.device, b, &mr);

			m_alloc_info.allocationSize = mr.size;
			m_alloc_info.memoryTypeIndex = find_memory_type(mr.memoryTypeBits, properties);
			
			VkDeviceMemory mem;
			if (vkAllocateMemory(m_base.device, &m_alloc_info, m_alloc, &mem) != VK_SUCCESS)
				throw std::runtime_error("failed to allocate memory!");

			vkBindBufferMemory(m_base.device, b, mem, 0);

			return mem;
		}

		VkDeviceMemory allocate(VkImage im, VkMemoryPropertyFlags properties)
		{
			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, im, &mr);

			m_alloc_info.allocationSize = mr.size;
			m_alloc_info.memoryTypeIndex = find_memory_type(mr.memoryTypeBits, properties);

			VkDeviceMemory mem;
			if (vkAllocateMemory(m_base.device, &m_alloc_info, m_alloc, &mem) != VK_SUCCESS)
				throw std::runtime_error("failed to allocate memory!");

			vkBindImageMemory(m_base.device, im, mem, 0);

			return mem;
		}

		VkDeviceMemory allocate(VkDeviceSize size, uint32_t memory_type_index)
		{
			VkDeviceMemory mem;
			m_alloc_info.memoryTypeIndex = memory_type_index;
			m_alloc_info.size = size;
			if (vkAllocateMemory(m_base.device, &m_alloc_info, m_alloc, &mem) != VK_SUCCESS)
				throw std::runtime_error("failed to allocate memory!");

			return mem;
		}

		void free(VkDeviceMemory mem)
		{
			vkFreeMemory(m_base.device, mem, m_alloc);
		}
	
		uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties)
		{
			for (uint32_t i = 0; i < m_memory_properties.memoryTypeCount; ++i)
			{
				if ((type_filter & (1 << i)) && ((m_memory_properties.memoryTypes[i].propertyFlags & properties) == properties))
					return i;
			}

			throw std::runtime_error("failed to find suitable memory type!");
		}

		static constexpr int n = sizeof(char*);
		static constexpr int m = sizeof(VkDeviceSize);

	private:
		VkPhysicalDeviceMemoryProperties m_memory_properties;
		VkMemoryAllocateInfo m_alloc_info;

		std::array<VkDeviceMemory, VK_MAX_MEMORY_TYPES> m_memory;


		device_memory(const base_info& base);

		static device_memory* m_instance;

		const base_info& m_base;

		/*static const size_t BLOCK_SIZE[USAGE_COUNT];
		size_t m_cell_size[USAGE_COUNT];
		VkMemoryAllocateInfo m_alloc_info[USAGE_COUNT];

		std::array<std::multimap<uint32_t, size_t>, USAGE_COUNT> m_available_cells;
		std::array<std::vector<VkDeviceMemory>, USAGE_COUNT> m_blocks;
		std::mutex m_block_mutexes[USAGE_COUNT];*/

		//allocator
		allocator m_alloc;
	};
}

