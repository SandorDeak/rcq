#pragma once

#include <stdexcept>

#include "memory_resource.h"

namespace rcq
{

	template<typename PType>
	class pool_resource : public memory_resource<PType>
	{
	public:
		pool_resource(PType block_size, memory_resource<PType>* upstream) :
			memory_resource(upstream)
		{
			m_next_chunk_block_count = 1;
			m_block_size = block_size;
		}

		~pool_resource()
		{
			for (PType i = 0; i < m_chunks.size(); ++i)
				m_upstream->deallocate(m_chunks[i]);
		}

		PType allocate(PType size, PType alignment = 1) override
		{
			if (!m_free_blocks.empty())
			{
				PType ret = m_free_blocks.top();
				m_free_blocks.pop();
				return ret;
			}

			PType new_chunk = m_upstream->allocate(m_block_size*m_next_chunk_block_count, m_alignment);
			m_chunks.push_back(new_chunk);
			for (PType i = 1; i < m_next_chunk_block_count; ++i)
			{
				new_chunk += m_block_size;
				m_free_block.push(new_chunk);
			}
			m_next_chunks_block_count *= 2;

			return *m_chunks.rbegin();
		}

		void deallocate(PType p) override
		{
			m_free_block.push(p);
		}

	private:
		PType m_block_size;
		PType m_alignment;
		stack<PType> m_free_blocks;
		vector<PType> m_chunks;
		PType m_next_chunk_block_count;
	};

}