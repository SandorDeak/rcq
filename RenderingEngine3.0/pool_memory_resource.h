#pragma once

#include "device_memory_resource.h"
#include "stack.h"

namespace rcq
{
	class pool_memory_resource : public device_memory_resource
	{
	public:
		pool_memory_resource() {}

		pool_memory_resource(uint64_t block_size, uint64_t block_alignment, device_memory_resource* upstream,
			memory_resource* metadata_memory_resource) :
			device_memory_resource(block_alignment, upstream->device(), upstream->handle(), upstream),
			m_chunks(metadata_memory_resource),
			m_free_blocks(metadata_memory_resource),
			m_next_chunk_size(block_size)
		{
			assert(block_size%block_alignment == 0);
		}

		void init(uint64_t block_size, uint64_t block_alignment, device_memory_resource* upstream,
			memory_resource* metadata_memory_resource)
		{
			device_memory_resource::init(block_alignment, upstream->device(), upstream->handle(), upstream);

			m_chunks.init(metadata_memory_resource);
			m_free_blocks.init(metadata_memory_resource);
			m_next_chunk_size = block_size;

			assert(block_size%block_alignment == 0);
		}

		~pool_memory_resource()
		{
			while (!m_chunks.empty())
			{
				m_upstream->deallocate(*m_chunks.top());
				m_chunks.pop();
			}
		}

		uint64_t allocate(uint64_t size, uint64_t alignment) override
		{
			assert(size == m_block_size && alignment == m_block_alignment);

			if (!m_free_blocks.empty())
			{
				uint64_t ret = *m_free_blocks.top();
				m_free_blocks.pop();
				return ret;
			}
			else
			{
				uint64_t block = m_upstream->allocate(m_next_chunk_size, m_block_alignment);
				*m_chunks.push() = block;
				uint64_t end = block + m_next_chunk_size;
				block += m_block_size;
				m_next_chunk_size <<= 1;
				while (block != end)
				{
					*m_free_blocks.push() = block;
					block += m_block_size;
				}
				return *m_chunks.top();
			}
		}

		void deallocate(uint64_t p) override
		{
			*m_free_blocks.push() = p;
		}

	private:
		uint64_t m_block_size;
		uint64_t m_block_alignment;
		stack<uint64_t> m_chunks;
		stack<uint64_t> m_free_blocks;
		uint64_t m_next_chunk_size;
	};
}