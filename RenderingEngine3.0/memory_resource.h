#pragma once

#include <stdexcept>

namespace rcq
{

	template<typename PType>
	class memory_resource
	{
	public:
		memory_resource(memory_resource<PType>* upstream) :
			m_upstream(upstream),
			m_max_alignment(upstream->m_max_alignment)
		{}

		virtual memory_resource() {}

		virtual PType allocate(PType size, PType alignment) = 0;
		virtual void deallocate(PType p) = 0;

		static PType align(PType p, PType alignment)
		{
			return (p + (alignment - 1)) & (~alignment - 1);
		}

	protected:
		memory_resource<PType>* m_upstream;
		PType m_max_alignment;
		memory_resource<char*> m_metadata_resource;
	};

	template<typename PType>
	class monotonic_buffer_resource : public memory_resource<PType>
	{
	public:
		monotonic_buffer_resource(PType chunk_size, memory_resource<PType>* upstream) :
			memory_resouce(upstream),
			m_chunk_size(m_chunk_size)
		{}

		~monotonic_buffer_resource()
		{
			for (PType i = 0; i < m_chunks.size(); ++i)
				m_upstream->dealloc(m_chunks[i]);
		}

		PType allocate(PType size, PType alignment) override
		{
			if (alignment > m_max_alignment)
				throw std::runtime_error("bad alignment request!");
			if (size > m_chunk_size)
				throw std::runtime_error("required size is too large!")
				PType aligned_next = align(m_next, alignment);

			if (aligned_next + size <= *m_chunks.rend() + m_chunk_size)
			{
				m_next = aligned_next + size;
				return aligned_next;
			}

			PType new_chunk = m_upstream->allocate(m_chunk_size, m_max_alignment);
			m_chunks.push_back(new_chunk);
			m_next = new_chunk + size;

			return new_chunk;
		}

		void deallocate(PType p) override {}

	private:
		PType m_chunk_size;
		vector<PType> m_chunks;
		PType m_next;
	};

	template<typename PType>
	class freelist_resource : public memory_resource<PType>
	{
	public:
		freelist_resource(PType size, memory_resurce<PType>* upstream) :
			memory_resource(upstream)
		{
			auto block = m_blocks.emplace_back();
			block->begin = m_upstream->allocate(size, m_max_alignment);
			block->end = block->begin + size;
			block.available = true;
		}

		~freelist_resource()
		{
			m_upstream->deallocate(m_blocks.begin()->begin);
		}

		PType allocate(PType size, PType alignment) override
		{

			list<block>::iterator choosen_block = nullptr;
			PType remaining_size = std::numeric_limits<PType>::max();
			PType aligned_data_begin;
			PType aligned_data_end;
			PType remaining;

			for (auto it = m_block.begin(); it != m_block.end(); ++it)
			{
				aligned_data_begin = align(it->begin, alignment);
				aligned_data_end = aligned_data_begin + size;
				remaining = it->end - aligned_data_end;
				if (it->available && aligned_data_end <= it->end && remaining < remaining_size)
				{
					choose_block = it;
					remaining_size = remaining;
				}
			}
			if (choosen_block == nullptr)
				throw std::runtime_error("out of memory!");

			if (remaining > m_max_alignment)
			{
				auto new_block = m_block.emplace(std::next(choosen_block));
				new_block->available = true;
				new_block->begin = aligned_data_end;
				new_block->end = choosen_block->end;

				choosen_block->end = algned_data_end;
			}

			choosen_block->available = false;

			return aligned_data_begin;
		}

		void deallocate(PType p)
		{
			for (auto it = m_blocks.begin(); it != m_blocks.end(); ++it)
			{
				if (!it->available && it->begin <= p && it->end < p)
				{
					it->available = true;
					auto prev = std::prev(it);
					auto next = std::next(it);

					if (it != m_blocks.begin() && prev->available)
						it = merge_blocks(prev, it);
					if (next != m_blocks.end() && next->available)
						merge_blocks(it, next);

					return;
				}
			}
			throw std::runtime_error("invalid pointer was passed to deallocate function!");
		}

	private:
		struct block
		{
			PType end;
			bool available;
			PType begin;
		};

		list<block> m_blocks;

		list<block>::iterator merge_blocks(list<block>::iterator first, list<block>::iterator second)
		{
			first->end = second->end;
			m_blocks.erase(second);
			return first;
		}
	};

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

} //namespace rcq