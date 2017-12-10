#pragma once

#include "device_memory.h"
#include "stack.h"
#include "host_memory.h"
namespace rcq
{
	class pool_device_memory : public device_memory
	{
	public:
		pool_device_memory() {}

		pool_device_memory(uint64_t block_size, uint64_t block_alignment, device_memory* upstream,
			host_memory* metadata_memory_resource) :
			device_memory(block_alignment, upstream->device(), upstream->handle_ptr(), upstream),
			m_chunks(metadata_memory_resource),
			m_free_blocks(metadata_memory_resource),
			m_next_chunk_size(block_size)
		{
			assert(block_size%block_alignment == 0);
		}

		void init(VkDeviceSize block_size, VkDeviceSize block_alignment, device_memory* upstream,
			host_memory* metadata_memory_resource)
		{
			device_memory::init(block_alignment, upstream->device(), upstream->handle_ptr(), upstream);

			m_chunks.init(metadata_memory_resource);
			m_free_blocks.init(metadata_memory_resource);
			m_next_chunk_size = block_size;

			assert(block_size%block_alignment == 0);
		}

		~pool_device_memory()
		{
			while (!m_chunks.empty())
			{
				m_upstream->deallocate(*m_chunks.top());
				m_chunks.pop();
			}
		}

		VkDeviceSize allocate(VkDeviceSize size, VkDeviceSize alignment) override
		{
			assert(size == m_block_size && alignment <= m_block_alignment);

			if (!m_free_blocks.empty())
			{
				VkDeviceSize ret = *m_free_blocks.top();
				m_free_blocks.pop();
				return ret;
			}
			else
			{
				VkDeviceSize block = m_upstream->allocate(m_next_chunk_size, m_block_alignment);
				*m_chunks.push() = block;
				VkDeviceSize end = block + m_next_chunk_size;
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

		void deallocate(VkDeviceSize p) override
		{
			*m_free_blocks.push() = p;
		}

	private:
		VkDeviceSize m_block_size;
		VkDeviceSize m_block_alignment;
		stack<VkDeviceSize> m_chunks;
		stack<VkDeviceSize> m_free_blocks;
		VkDeviceSize m_next_chunk_size;
	};
}