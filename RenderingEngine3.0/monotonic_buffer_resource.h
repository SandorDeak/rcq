#pragma once

#include<stdexcept>

#include "device_memory_resource.h"
#include "vector.h"

//this doesnt work currently!!!

namespace rcq
{
	class monotonic_buffer_resource : public device_memory_resource
	{
	public:
		monotonic_buffer_resource(uint64_t chunk_size, uint64_t alignment, device_memory_resource* upstream,
			memory_resource* metadata_memory_resource) :
			device_memory_resource(alignment, upstream->device(), upstream->handle(), upstream),
			m_chunk_size(chunk_size),
			m_chunks(metadata_memory_resource)
		{
			*m_chunks.push_back() = m_upstream->allocate(m_chunk_size, m_max_alignment);
			m_next = *m_chunks.last();
		}

		~monotonic_buffer_resource()
		{
			for (size_t i = 0; i < m_chunks.size(); ++i)
				m_upstream->deallocate(*m_chunks[i]);
		}

		uint64_t allocate(uint64_t size, uint64_t alignment) override
		{
			if (alignment > m_max_alignment)
				throw std::runtime_error("bad alignment request!");
			if (size > m_chunk_size)
				throw std::runtime_error("required size is too large!");

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

		void deallocate(uint64_t p) override {}

	private:
		uint64_t m_chunk_size;
		vector<uint64_t> m_chunks;
		uint64_t m_next;
	};
}