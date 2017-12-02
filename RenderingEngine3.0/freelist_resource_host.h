#pragma once

#include "memory_resource.h"
#include <limits>
#include <stdexcept>
#include <assert.h>

namespace rcq
{

	class freelist_resource_host : public memory_resource
	{
	public:
		freelist_resource_host(uint64_t size, uint64_t max_alignment, memory_resource* upstream) :
			memory_resource(max_alignment<alignof(block) ? alignof(block) : max_alignment, upstream)
		{
			static_assert(sizeof(block) % alignof(block) == 0);

			assert(m_max_alignment < m_upstream->max_alignment());
			assert(size >= 3 * sizeof(block));

			uint64_t begin = m_upstream->allocate(size, max_alignment);
			m_begin = reinterpret_cast<block*>(begin);
			m_end = m_begin + 1;

			m_begin->free = false;
			m_end->free = false;

			block* b = m_end + 1;
			b->begin = begin + 2 * sizeof(block);
			b->end = begin + size;
			b->free = true;
			b->prev = m_begin;
			b->next = m_end;
			b->prev_free = m_begin;
			b->next_free = m_end;

			m_begin->next = b;
			m_end->prev = b;
			m_begin->next_free = b;
			m_end->prev_free = b;

		}

		~freelist_resource_host()
		{
			m_upstream->deallocate(m_begin->begin);
		}

		uint64_t allocate(uint64_t size, uint64_t alignment) override
		{

			assert(alignment <= m_max_alignment);

			alignment = alignment < alignof(block) ? alignof(block) : alignment;
			size = align(size, alignof(block));

			block* choosen_block = nullptr;
			uint64_t remaining = std::numeric_limits<size_t>::max();
			uint64_t aligned_begin;
			uint64_t aligned_end;

			block* b = m_begin->next_free;
			while (b != m_end)
			{
				uint64_t temp_aligned_begin = align(b->begin + sizeof(block), alignment);
				uint64_t temp_aligned_end = temp_aligned_begin + size;
				uint64_t temp_remaining = b->end - temp_aligned_end;
				if (temp_aligned_end <= b->end && temp_remaining < remaining)
				{
					choosen_block = b;
					remaining = temp_remaining;
					aligned_begin = temp_aligned_begin;
					aligned_end = temp_aligned_end;
					if (remaining < sizeof(block) + m_max_alignment)
						break;
				}
				b = b->next_free;
			}

			if (choosen_block == nullptr)
				throw std::runtime_error("out of memory!");

			if (reinterpret_cast<size_t>(choosen_block) + sizeof(block) != aligned_begin)
			{
				block temp;
				memcpy(&temp, choosen_block, sizeof(block));
				choosen_block = reinterpret_cast<block*>(aligned_begin - sizeof(block));
				memcpy(choosen_block, &temp, sizeof(block));

				choosen_block->next->prev = choosen_block;
				choosen_block->prev->next = choosen_block;
			}

			choosen_block->free = false;
			choosen_block->prev_free->next_free = choosen_block->next_free;
			choosen_block->next_free->prev_free = choosen_block->prev_free;

			if (remaining > sizeof(block) + m_max_alignment)
			{
				block* new_block = reinterpret_cast<block*>(aligned_end);
				new_block->prev = choosen_block;
				new_block->begin = aligned_end;
				new_block->end = choosen_block->end;
				new_block->next = choosen_block->next;
				new_block->free = true;
				new_block->next_free = m_begin->next_free;
				new_block->prev_free = m_begin;

				new_block->next_free->prev_free = new_block;
				new_block->prev_free->next_free = new_block;
				new_block->next->prev = new_block;
				new_block->prev->next = new_block;
				
				choosen_block->end = aligned_end;
			}


			return aligned_begin;
		}

		void deallocate(uint64_t p) override
		{
			block* dealloc_block = reinterpret_cast<block*>(p - sizeof(block));

			if (dealloc_block->next->free && dealloc_block->prev->free)
			{
				dealloc_block->prev->next = dealloc_block->next->next;
				dealloc_block->prev->next->prev = dealloc_block->prev;
				dealloc_block->prev->end = dealloc_block->next->end;
				
				dealloc_block->next->prev_free->next_free = dealloc_block->next->next_free;
				dealloc_block->next->next_free->prev_free = dealloc_block->next->prev_free;
			}
			else if (dealloc_block->next->free)
			{
				dealloc_block->next->prev = dealloc_block->prev;
				dealloc_block->prev->next = dealloc_block->next;
				dealloc_block->next->begin = dealloc_block->begin;
			}
			else if (dealloc_block->prev->free)
			{
				dealloc_block->prev->next = dealloc_block->next;
				dealloc_block->next->prev = dealloc_block->prev;
				dealloc_block->prev->end = dealloc_block->end;
			}
			else
			{
				dealloc_block->free = true;
				dealloc_block->next_free = m_begin->next_free;
				dealloc_block->prev_free = m_begin;
				dealloc_block->next_free->prev_free = dealloc_block;
				dealloc_block->prev_free->next_free = dealloc_block;
			}
		}

	private:
		struct block
		{
			block* prev;
			block* next;
			block* next_free;
			block* prev_free;
			uint64_t begin;
			uint64_t end;
			bool free;

		};
		block* m_begin;
		block* m_end;
	};
}