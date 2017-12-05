#pragma once

#include <stdexcept>

#include "memory_resource.h"

namespace rcq
{

	class pool_memory_resource_host : public memory_resource
	{
	public:
		pool_memory_resource_host(uint64_t block_size, uint64_t alignment, memory_resource* upstream) :
			memory_resource(alignment<min_alignment() ? min_alignment() : alignment, upstream),
			m_next_chunk_size(block_size),
			m_block_size(block_size),
			m_chunks(nullptr),
			m_free_blocks(nullptr)			
		{
			static_assert(sizeof(chunk) % alignof(chunk) == 0 && sizeof(block) % alignof(block) == 0);
			assert(alignment <= upstream->max_alignment() && block_size%alignment==0 && block_size>=sizeof(block));
		}

		~pool_memory_resource_host()
		{
			while (m_chunks != nullptr)
			{
				chunk* next = m_chunks->next;
				m_upstream->deallocate(reinterpret_cast<uint64_t>(m_chunks));
				m_chunks = next;
			}
		}

		uint64_t allocate(uint64_t size=0, uint64_t alignment = 0) override
		{
			if (m_free_blocks!=nullptr)
			{
				uint64_t ret = reinterpret_cast<uint64_t>(m_free_blocks);
				m_free_blocks = m_free_blocks->next;
				return ret;
			}

			uint64_t new_data = m_upstream->allocate(m_next_chunk_size+sizeof(chunk), m_max_alignment);
			chunk* new_chunk = reinterpret_cast<chunk*>(new_data);
			new_chunk->next = m_chunks;
			m_chunks = new_chunk;

			new_data += sizeof(chunk);
			uint64_t new_data_end = new_data + m_next_chunk_size;
			uint64_t ret = new_data;
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

		void deallocate(uint64_t p) override
		{
			block* b = reinterpret_cast<block*>(p);
			b->next = m_free_blocks;
			m_free_blocks = b;
		}

	private:
		constexpr uint64_t min_alignment()
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

		uint64_t m_block_size;			
		uint64_t m_next_chunk_size;
	};

}