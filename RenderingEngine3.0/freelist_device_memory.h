#pragma once

#include <assert.h>

#include "device_memory.h"
#include "host_memory.h"

namespace rcq
{
	class freelist_device_memory : public device_memory
	{
	public:
		freelist_device_memory() {}

		freelist_device_memory(VkDeviceSize size, VkDeviceSize alignment, device_memory* upstream,
			host_memory* metadata_memory) :
			device_memory(alignment, upstream->device(), upstream->handle_ptr(), upstream),
			m_metadata_memory(metadata_memory)
		{
			static_assert(sizeof(block) % alignof(block) == 0);

			assert(m_max_alignment <= m_upstream->max_alignment());
			assert(alignof(block) <= m_metadata_memory->max_alignment());

			m_begin = reinterpret_cast<block*>(m_metadata_memory->allocate(2 * sizeof(block), alignof(block)));
			m_end = m_begin + 1;


			VkDeviceSize data = m_upstream->allocate(size, alignment);
			block* b = reinterpret_cast<block*>(m_metadata_memory->allocate(2 * sizeof(block), alignof(block)));
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

		void init(VkDeviceSize size, VkDeviceSize alignment, device_memory* upstream,
			host_memory* metadata_memory)
		{
			device_memory::init(alignment, upstream->device(), upstream->handle_ptr(), upstream);
			m_metadata_memory = metadata_memory;

			assert(m_max_alignment <= m_upstream->max_alignment());
			assert(alignof(block) <= m_metadata_memory->max_alignment());

			m_begin = reinterpret_cast<block*>(m_metadata_memory->allocate(2 * sizeof(block), alignof(block)));
			m_end = m_begin + 1;


			VkDeviceSize data = m_upstream->allocate(size, alignment);
			block* b = reinterpret_cast<block*>(m_metadata_memory->allocate(sizeof(block), alignof(block)));;
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

		void reset()
		{
			m_upstream->deallocate(m_begin->next->begin);
			while (m_begin->next != m_end)
			{
				block* next = m_begin->next->next;
				m_metadata_memory->deallocate(reinterpret_cast<size_t>(m_begin->next));
				m_begin->next = next;
			}
			while (m_end->next != m_begin)
			{
				block* next = m_end->next->next;
				m_metadata_memory->deallocate(reinterpret_cast<size_t>(m_end->next));
				m_end->next = next;
			}
			m_metadata_memory->deallocate(reinterpret_cast<size_t>(m_begin));
		}

		~freelist_device_memory()
		{}

		VkDeviceSize allocate(VkDeviceSize size, VkDeviceSize alignment) override
		{
			assert(alignment <= m_max_alignment);

			alignment = alignment < alignof(block) ? alignof(block) : alignment;
			size = align(size, alignof(block));

			block* choosen_block = nullptr;
			VkDeviceSize remaining = std::numeric_limits<VkDeviceSize>::max();
			VkDeviceSize aligned_begin;
			VkDeviceSize aligned_end;

			block* b = m_begin->next_free;
			while (b != m_end)
			{
				VkDeviceSize temp_aligned_begin = align(b->begin, alignment);
				VkDeviceSize temp_aligned_end = temp_aligned_begin + size;
				VkDeviceSize temp_remaining = b->end - temp_aligned_end;
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

			assert(choosen_block != nullptr);

			choosen_block->free = false;
			choosen_block->prev_free->next_free = choosen_block->next_free;
			choosen_block->next_free->prev_free = choosen_block->prev_free;

			choosen_block->next_res = m_begin->next_res;
			choosen_block->prev_res = m_begin;
			choosen_block->prev_res->next_res = choosen_block;
			choosen_block->next_res->prev_res = choosen_block;

			if (remaining > 2*sizeof(block))
			{
				block* new_block;
				if (m_end->next == m_begin)
				{
					new_block = reinterpret_cast<block*>(m_metadata_memory->allocate(sizeof(block), alignof(block)));
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

		void deallocate(VkDeviceSize p)
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
			VkDeviceSize begin;
			VkDeviceSize end;
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

		host_memory* m_metadata_memory;
	};
}