#pragma once

#include<stdexcept>
#include <assert.h>
#include "host_memory.h"

namespace rcq
{
	class monotonic_buffer_host_memory : public host_memory
	{
	public:
		monotonic_buffer_host_memory() {}

		monotonic_buffer_host_memory(size_t first_chunks_size, size_t max_alignment, host_memory* upstream) :
			host_memory(max_alignment < alignof(chunk) ? alignof(chunk) : max_alignment, upstream),
			m_next_chunk_size(first_chunks_size)
		{
			assert(m_max_alignment <= m_upstream->max_alignment());
			m_begin = m_upstream->allocate(m_next_chunk_size, m_max_alignment);
			m_end = m_begin + m_next_chunk_size;
			m_next_chunk_size <<= 1;
			m_first_chunk = reinterpret_cast<chunk*>(m_begin);
			m_last_chunk = m_first_chunk;
			m_last_chunk->next = nullptr;
			m_begin += sizeof(chunk);
		}

		void init(size_t first_chunks_size, size_t max_alignment, host_memory* upstream)
		{
			host_memory::init(max_alignment < alignof(chunk) ? alignof(chunk) : max_alignment, upstream);
			m_next_chunk_size = first_chunks_size;

			assert(m_max_alignment <= m_upstream->max_alignment());
			m_begin = m_upstream->allocate(m_next_chunk_size, m_max_alignment);
			m_end = m_begin + m_next_chunk_size;
			m_next_chunk_size <<= 1;
			m_first_chunk = reinterpret_cast<chunk*>(m_begin);
			m_last_chunk = m_first_chunk;
			m_last_chunk->next = nullptr;
			m_begin += sizeof(chunk);
		}

		void reset()
		{
			while (m_first_chunk != nullptr)
			{
				chunk* next = m_first_chunk->next;
				m_upstream->deallocate(reinterpret_cast<size_t>(m_first_chunk));
				m_first_chunk = next;
			}
		}

		~monotonic_buffer_host_memory()
		{}

		size_t allocate(size_t size, size_t alignment) override
		{
			assert(alignment <= m_max_alignment);

			m_begin = align(m_begin, alignment);
			if (m_begin + size > m_end)
			{

				while (m_next_chunk_size < size+m_max_alignment+sizeof(chunk))
					m_next_chunk_size <<= 1;

				m_begin = m_upstream->allocate(m_next_chunk_size, m_max_alignment);
				m_last_chunk->next = reinterpret_cast<chunk*>(m_begin);
				m_last_chunk = m_last_chunk->next;
				m_last_chunk->next = nullptr;
				m_end = m_begin + m_next_chunk_size;
				size_t ret = align(m_begin+sizeof(chunk), alignment);
				m_begin = ret+size;
				m_next_chunk_size <<= 1;
				return ret;
			}
			else
			{
				size_t ret = m_begin;
				m_begin += size;
				return ret;
			}
		}

		void deallocate(size_t p) override {}

	private:
		struct chunk
		{
			chunk* next;
		};

		size_t m_next_chunk_size;
		size_t m_end;
		size_t m_begin;
		chunk* m_first_chunk;
		chunk* m_last_chunk;
	};
}