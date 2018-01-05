#pragma once


#include "host_memory.h"

namespace rcq
{

	class pool_host_memory : public host_memory
	{
	public:
		pool_host_memory() {}

		pool_host_memory(size_t block_size, size_t alignment, host_memory* upstream) :
			host_memory(alignment<min_alignment() ? min_alignment() : alignment, upstream),
			m_next_chunk_size(block_size),
			m_block_size(block_size),
			m_chunks(nullptr),
			m_free_blocks(nullptr)			
		{
			static_assert(sizeof(chunk) % alignof(chunk) == 0 && sizeof(block) % alignof(block) == 0);
			assert(alignment <= upstream->max_alignment() && block_size%alignment==0 && block_size>=sizeof(block));
		}

		void init(size_t block_size, size_t alignment, host_memory* upstream)
		{
			host_memory::init(alignment<min_alignment() ? min_alignment() : alignment, upstream);
			m_next_chunk_size = block_size;
			m_block_size = block_size;
			m_chunks = nullptr;
			m_free_blocks = nullptr;
		}

		void reset()
		{
			while (m_chunks != nullptr)
			{
				chunk* next = m_chunks->next;
				m_upstream->deallocate(reinterpret_cast<size_t>(m_chunks));
				m_chunks = next;
			}
		}

		~pool_host_memory()
		{}

		size_t allocate(size_t size, size_t alignment) override
		{
			assert(size == m_block_size && alignment <= m_max_alignment);

			if (m_free_blocks!=nullptr)
			{
				size_t ret = reinterpret_cast<size_t>(m_free_blocks);
				m_free_blocks = m_free_blocks->next;
				return ret;
			}

			size_t new_data = m_upstream->allocate(m_next_chunk_size+sizeof(chunk), m_max_alignment);
			chunk* new_chunk = reinterpret_cast<chunk*>(new_data);
			new_chunk->next = m_chunks;
			m_chunks = new_chunk;

			new_data += sizeof(chunk);
			size_t new_data_end = new_data + m_next_chunk_size;
			size_t ret = new_data;
			new_data += m_block_size;
			while (new_data != new_data_end)
			{
				block* b = reinterpret_cast<block*>(new_data);
				b->next = m_free_blocks;
				m_free_blocks = b;
				new_data += m_block_size;
			}

			m_next_chunk_size <<= 1;

			return ret;
		}

		void deallocate(size_t p) override
		{
			block* b = reinterpret_cast<block*>(p);
			b->next = m_free_blocks;
			m_free_blocks = b;
		}

	private:
		constexpr size_t min_alignment()
		{
			return alignof(chunk) < alignof(block) ? alignof(block) : alignof(chunk);
		}


		struct chunk
		{
			chunk* next;
		};

		struct block
		{
			block* next;
		};

		chunk* m_chunks;
		block* m_free_blocks;

		size_t m_block_size;			
		size_t m_next_chunk_size;
	};

}