#pragma once

#include "host_memory.h"
#include "vector.h"
#include <stdint.h>

namespace rcq
{
	struct slot
	{
		uint32_t index;
		uint32_t generation;
	};

	template<typename T>
	class slot_map
	{
	public:
		slot_map() {}

		slot_map(size_t chunk_size, host_memory* memory) :
			m_memory(memory),
			m_chunks(memory),
			m_size(0),
			m_capacity(0),
			m_chunk_size(chunk_size)
		{}

		slot_map& operator=(const slot_map&) = delete;

		void init(uint32_t chunk_size, host_memory* memory)
		{
			m_memory = memory;
			m_chunks.init(memory);
			m_size = 0;
			m_capacity = 0;
			m_chunk_size = chunk_size;
		}
		slot_map(slot_map&& other) :
			m_memory(other.m_memory),
			m_chunks(std::move(other.m_chunks)),
			m_size(other.m_size),
			m_capacity(other.m_capacity),
			m_next_slot(other.m_next_slot)
		{
			other.m_size = 0;
			other.m_capacity = 0;
		}

		~slot_map()
		{
			reset();
		}

		void reset()
		{
			for (auto it = m_chunks.begin(); it != m_chunks.end(); ++it)
			{
				m_memory->deallocate(reinterpret_cast<size_t>(it->data));
			}
			m_chunks.reset();
		}

		slot_map& operator=(slot_map&& other)
		{
			for (auto it = m_chunks.begin(); it != m_chunks.end(); ++it)
			{
				m_memory->deallocate(reinterpret_cast<size_t>(it->data));
			}

			m_chunks = std::move(other.m_chunks);
			m_memory = other.m_memory;
			m_size = other.m_size;
			m_capacity = other.m_capacity;
			m_next_slot = other.m_next_slot;

			other.m_size = 0;
			other.m_capacity = 0;
		}

		T* push(slot& s)
		{
			if (m_size == m_capacity)
				extend();

			slot* next = get_slot(m_next_slot);

			s = { m_next_slot, next->generation };

			*(get_slot_view(m_size)) = next;

			m_next_slot = next->index;
			next->index = m_size;
			++m_size;

			return get_data(s.index);
		}

		bool destroy(slot s)
		{
			slot* real_slot = get_slot(s.index);
			if (real_slot->generation != s.generation)
				return false;

			T* deleted = get_data(real_slot->index);
			++real_slot->generation;
			--m_size;

			if (real_slot->index != m_size)
			{
				memcpy(deleted, get_data(m_size), sizeof(T));
				get_slot(m_size)->index = real_slot->index;
			}

			real_slot->index = m_next_slot;
			m_next_slot = s.index;
			return true;
		}

		T* get(slot s)
		{
			slot* real_slot = get_slot(s.index);
			if (real_slot->generation != s.generation)
				return nullptr;

			return get_data(real_slot->index);
		}

		const T* get(slot s) const
		{
			slot* real_slot = get_slot(s.index);
			if (real_slot->generation != s.generation)
				return nullptr;

			return get_data(real_slot->index);
		}

		uint32_t size() const
		{
			return m_size;
		}

		template<typename Callable>
		void for_each(Callable&& f)
		{
			uint32_t remaining = m_size;
			auto current_chunk = m_chunks.data();
			while (remaining>0)
			{
				auto data = current_chunk->data;
				uint32_t steps = remaining > m_chunk_size ? m_chunk_size : remaining;
				remaining -= steps;
				while (steps-- > 0)
					f(*data++);
				++current_chunk;
			}
		}
	private:
		static constexpr uint32_t chunk_size_bit_count = 8;
		static constexpr uint32_t chunk_size = 256;
		static constexpr uint32_t chunk_index_mask = chunk_size - 1;

		struct chunk
		{
			T* data;
			slot** slot_views;
			slot* slots;
		};

		vector<chunk> m_chunks;
		host_memory* m_memory;
		size_t m_chunk_size;
		size_t m_size;
		size_t m_capacity;
		uint32_t m_next_slot;

		void extend()
		{

			constexpr size_t alignment = alignof(T) < alignof(slot) ?  alignof(slot) : alignof(T);

			size_t new_chunk_pointer = m_memory->allocate(sizeof(T) + sizeof(slot*) + sizeof(slot)*chunk_size, alignment);

			auto new_chunk = m_chunks.push_back();
			new_chunk->data = reinterpret_cast<T*>(new_chunk_pointer);
			new_chunk->slot_views = reinterpret_cast<slot**>(new_chunk_pointer + sizeof(T)*chunk_size);
			new_chunk->slots = reinterpret_cast<slot*>(new_chunk_pointer + (sizeof(slot*) + sizeof(T))*chunk_size);

			m_next_slot = m_size;

			slot* id = new_chunk->slots;

			uint32_t i = m_capacity + 1;
			m_capacity += chunk_size;
			while (i < m_capacity)
			{
				id->index = i++;
				++id;
			}
		}

		T* get_data(uint32_t index)
		{
			return m_chunks[index >> chunk_size_bit_count].data + (index&chunk_index_mask);
		}

		slot* get_slot(uint32_t index)
		{
			auto i0 = index >> chunk_size_bit_count;
			auto i1 = index&chunk_size;
			return m_chunks[index >> chunk_size_bit_count].slots + (index&chunk_index_mask);
		}

		slot** get_slot_view(uint32_t index)
		{
			return m_chunks[index >> chunk_size_bit_count].slot_views + (index&chunk_index_mask);
		}


	};
} // namespace rcq