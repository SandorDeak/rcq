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

		cell_info alloc_buffer(USAGE usage, VkBuffer buffer, char** data);

		void free_buffer(USAGE usage, const cell_info& cell);

	private:
		device_memory(const base_info& base);

		static device_memory* m_instance;

		const base_info m_base;

		static const size_t BLOCK_SIZE[USAGE_COUNT];
		size_t m_cell_size;
		VkMemoryAllocateInfo m_alloc_info[USAGE_COUNT];

		std::array<std::multimap<uint32_t, size_t>, USAGE_COUNT> m_available_cells;
		std::array<std::vector<VkDeviceMemory>, USAGE_COUNT> m_blocks;
		std::mutex m_block_mutexes[USAGE_COUNT];
	};
}

