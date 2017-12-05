#pragma once

#include<stdexcept>
#include <assert.h>

#include "device_memory_resource.h"

namespace rcq
{
	class freelist_resource : public device_memory_resource
	{
	public:
		freelist_resource(uint64_t size, uint64_t alignment, device_memory_resource* upstream,
			memory_resource* metadata_memory_resource) :
			device_memory_resource(alignment, upstream->device(), upstream->handle(), upstream),
			m_metadata_resource(metadata_memory_resource)
		{
			static_assert(sizeof(block) % alignof(block) == 0);

			assert(m_max_alignment <= m_upstream->max_alignment());
			assert(alignof(block) <= m_metadata_resource->max_alignment());

			m_begin = reinterpret_cast<block*>(m_metadata_resource->allocate(2 * sizeof(block), alignof(block)));
			m_end = m_begin + 1;


			uint64_t data = m_upstream->allocate(size, alignment);
			block* b = reinterpret_cast<block*>(m_metadata_resource->allocate(2 * sizeof(block), alignof(block)));;
			b->begin = data;
			b->end = data + size;
			b->prev = m_begin;
			b->next = m_end;
			b->prev_free = m_begin;
			b->next_free = m_end;
			b->free = true;
			
			m_begin->next = b;
			m_begin->next_free = b;
			m_begin->free = false;
			m_begin->prev = m_end;
			m_begin->next_res = m_end;
			
			m_end->prev = b;
			m_end->next = m_begin;
			m_end->prev_free = b;
			m_end->prev_res = m_begin;
			m_end->free = false;
		}

		~freelist_resource()
		{
			m_upstream->deallocate(m_begin->next->begin);
			while (m_begin->next != m_end)
			{
				block* next = m_begin->next->next;
				m_metadata_resource->deallocate(reinterpret_cast<size_t>(m_begin->next));
				m_begin->next = next;
			}
			while (m_end->next != m_begin)
			{
				block* next = m_end->next->next;
				m_metadata_resource->deallocate(reinterpret_cast<size_t>(m_end->next));
				m_end->next = next;
			}
			m_metadata_resource->deallocate(reinterpret_cast<size_t>(m_begin));
		}

		uint64_t allocate(uint64_t size, uint64_t alignment) override
		{
			assert(alignment <= m_max_alignment);

			alignment = alignment < alignof(block) ? alignof(block) : alignment;
			size = align(size, alignof(block));

			block* choosen_block = nullptr;
			uint64_t remaining = std::numeric_limits<uint64_t>::max();
			uint64_t aligned_begin;
			uint64_t aligned_end;

			block* b = m_begin->next_free;
			while (b != m_end)
			{
				uint64_t temp_aligned_begin = align(b->begin, alignment);
				uint64_t temp_aligned_end = temp_aligned_begin + size;
				uint64_t temp_remaining = b->end - temp_aligned_end;
				if (temp_aligned_end <= b->end && temp_remaining < remaining)
				{
					choosen_block = b;
					remaining = temp_remaining;
					aligned_begin = temp_aligned_begin;
					aligned_end = temp_aligned_end;
					if (remaining < m_max_alignment)
						break;
				}
				b = b->next_free;
			}

			if (choosen_block == nullptr)
				throw std::runtime_error("out of memory!");

			choosen_block->free = false;
			choosen_block->prev_free->next_free = choosen_block->next_free;
			choosen_block->next_free->prev_free = choosen_block->prev_free;

			choosen_block->next_res = m_begin->next_res;
			choosen_block->prev_res = m_begin;
			choosen_block->prev_res->next_res = choosen_block;
			choosen_block->next_res->prev_res = choosen_block;

			if (remaining > m_max_alignment)
			{
				block* new_block;
				if (m_end->next == m_begin)
				{
					new_block = reinterpret_cast<block*>(m_metadata_resource->allocate(sizeof(block), alignof(block)));
				}
				else
				{
					new_block = m_end->next;
					m_end->next = m_end->next->next;
				}

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

		void deallocate(uint64_t p)
		{
			block* dealloc_block=m_begin->next_res;
			while (!(dealloc_block->begin <= p && p < dealloc_block->end))
				dealloc_block = dealloc_block->next_res;

			dealloc_block->next_res->prev_res = dealloc_block->prev_res;
			dealloc_block->prev_res->next_res = dealloc_block->next_res;

			if (dealloc_block->next->free && dealloc_block->prev->free)
			{
				dealloc_block->prev->next = dealloc_block->next->next;
				dealloc_block->prev->next->prev = dealloc_block->prev;
				dealloc_block->prev->end = dealloc_block->next->end;

				dealloc_block->next->prev_free->next_free = dealloc_block->next->next_free;
				dealloc_block->next->next_free->prev_free = dealloc_block->next->prev_free;

				dealloc_block->next->next = m_end->next;
				m_end->next = dealloc_block;
			}
			else if (dealloc_block->next->free)
			{
				dealloc_block->next->prev = dealloc_block->prev;
				dealloc_block->prev->next = dealloc_block->next;
				dealloc_block->next->begin = dealloc_block->begin;

				dealloc_block->next = m_end->next;
				m_end->next = dealloc_block;
			}
			else if (dealloc_block->prev->free)
			{
				dealloc_block->prev->next = dealloc_block->next;
				dealloc_block->next->prev = dealloc_block->prev;
				dealloc_block->prev->end = dealloc_block->end;

				dealloc_block->next = m_end->next;
				m_end->next = dealloc_block;
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
			uint64_t begin;
			uint64_t end;
			block* prev;
			block* next;
			block* prev_free;
			block* next_free;
			block* prev_res;
			block* next_res;
			bool free;
		};

		block* m_begin;
		block* m_end;

		memory_resource* m_metadata_resource;
	};
}