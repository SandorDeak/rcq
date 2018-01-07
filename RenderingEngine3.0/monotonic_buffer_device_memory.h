#pragma once

#include<stdexcept>

#include "device_memory.h"
#include "host_memory.h"
#include "vector.h"


namespace rcq
{
	class monotonic_buffer_device_memory : public device_memory
	{
	public:
		monotonic_buffer_device_memory() {}

		monotonic_buffer_device_memory(VkDeviceSize chunk_size, VkDeviceSize alignment, device_memory* upstream,
			host_memory* metadata_memory) :
			device_memory(alignment, upstream->device(), upstream->handle_ptr(), upstream),
			m_chunk_size(chunk_size),
			m_chunks(metadata_memory)
		{
			*m_chunks.push_back() = m_upstream->allocate(m_chunk_size, m_max_alignment);
			m_next = *m_chunks.last();
		}

		void init(VkDeviceSize chunk_size, VkDeviceSize alignment, device_memory* upstream,
			host_memory* metadata_memory_resource)
		{
			device_memory::init(alignment, upstream->device(), upstream->handle_ptr(), upstream);
			m_chunk_size = chunk_size;
			m_chunks.init(metadata_memory_resource);

			*m_chunks.push_back() = m_upstream->allocate(m_chunk_size, m_max_alignment);
			m_next = *m_chunks.last();
		}

		void reset()
		{
			for (size_t i = 0; i < m_chunks.size(); ++i)
				m_upstream->deallocate(m_chunks[i]);
			m_chunks.reset();
		}

		void clear()
		{
			if (!m_chunks.empty())
			{
				for (size_t i = 1; i < m_chunks.size(); ++i)
					m_upstream->deallocate(m_chunks[i]);
				m_chunks.resize(1);
				m_next = *m_chunks.last();
			}
		}

		~monotonic_buffer_device_memory()
		{}

		VkDeviceMemory allocate(VkDeviceMemory size, VkDeviceMemory alignment) override
		{
			assert(alignment <= m_max_alignment);
			assert(size <= m_chunk_size);

			uint64_t aligned_next = align(m_next, alignment);

			if (aligned_next + size <= *m_chunks.last() + m_chunk_size)
			{
				m_next = aligned_next + size;
				return aligned_next;
			}

			*m_chunks.push_back() = m_upstream->allocate(m_chunk_size, m_max_alignment);
			m_next = *m_chunks.last() + size;

			return *m_chunks.last();
		}

		void deallocate(VkDeviceSize p) override {}

	private:
		VkDeviceSize m_chunk_size;
		vector<VkDeviceSize> m_chunks;
		VkDeviceSize m_next;
	};
}