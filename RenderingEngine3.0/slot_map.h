#pragma once

#include "memory_resource.h"
#include "vector.h"
#include <stdint.h>

namespace rcq
{
	template<typename T>
	class slot_map
	{
	public:
		struct slot
		{
			uint32_t index;
			uint32_t generation;
		};

		slot_map(memory_resource<size_t>* memory) :
			m_memory(memory)
		{}

		slot_map(const slot_map&) = delete;

		slot_map(slot_map&& other) :
			m_chunks(other.m_chunks),
			m_chunks_size(other.m_chunks_size),
			m_chunks_capacity(other.m_chunks_capacity),
			m_size(other.m_size),
			m_capacity(other.m_capacity),
			m_next(other.m_next)
		{
			other.release();
		}

		slot_map& operator=(slot_map&& other)
		{
			if (this == *other)
				return;

			if (m_chunks != nullptr)
			{
				for_each([](T& t) { t.~T(); });

				for (uint32_t i = 0; i < m_chunks_size; ++i)
					std::free(m_chunks[i].data);

				std::free(m_chunks);
			}

			m_chunks = other.m_chunks;
			m_chunks_size = other.m_chunks;
			m_chunks_capacity = other.m_chunks_capacity;
			m_size = other.m_size;
			m_capacity = other.m_capacity;
			m_next = other.m_next;

			other.m_chunks = nullptr;
			m_chunks_size = 0;
			m_chunks_capacity = 0;
			m_size = 0;
			m_capacity = 0;
		}

		~slot_map()
		{
			if (m_chunks == nullptr)
				return;

			if constexpr(dealloc == dealloc_pattern::destruct)
			{
				for_each([](T& t) { t.~T(); });
			}

			for (uint32_t i = 0; i < m_chunks_size; ++i)
				std::free(m_chunks[i].data);

			std::free(m_chunks);
		}

		void swap(slot_map& other)
		{
			if (this == *other)
				return;

			auto chunks = m_chunks;
			auto chunks_size = m_chunks_size;
			auto chunks_capacity = m_chunks_capacity;
			auto size = m_size;
			auto capacity = m_capacity;
			auto next = m_next;

			m_chunks = other.m_chunks;
			m_chunks_size = other.m_chunks;
			m_chunks_capacity = other.m_chunks_capacity;
			m_size = other.m_size;
			m_capacity = other.m_capacity;
			m_next = other.m_next;

			other.m_chunks = chunks;
			other.m_chunks_size = chunks_size;
			other.m_chunks_capacity = capacity;
			other.m_size = size;
			other.m_capacity = capacity;
			other.m_next = next;
		}

		void reset()
		{
			if (m_chunks == nullptr)
				return;

			if constexpr(dealloc == dealloc_pattern::destruct)
			{
				for_each([](T& t) { t.~T(); });
			}


			for (uint32_t i = 0; i < m_chunks_size; ++i)
				std::free(m_chunks[i].data);

			std::free(m_chunks);

			release();
		}

		T* push(slot& s)
		{
			if (m_size == m_capacity)
				extend();

			slot* next = get_slot(m_next);

			s = { m_next, next->generation };

			*(get_slot_view(m_size)) = next;

			m_next = next->index;
			next->index = m_size;
			++m_size;

			return get_data(s.index);
		}


		T* get(const slot& id)
		{
			slot* real_id = get_slot(id.index);
			if (real_id->generation != id.generation)
				return nullptr;

			return get_data(real_id->index);
		}

		bool destroy(const slot& id)
		{
			slot* real_id = get_slot(id.index);
			if (real_id->generation != id.generation)
				return false;

			T* deleted = get_data(real_id->index);
			++real_id->generation;


			--m_size;

			if (real_id->index != m_size)
			{
				*deleted = std::move(*get_data(m_size));
				get_slot(m_size)->index = real_id->index;
			}

			real_id->index = m_next;
			m_next = id.index;
			return true;
		}

		template<typename Callable>
		void for_each(Callable&& f)
		{
			uint32_t remaining = m_size;
			auto current_chunk = m_chunks;
			uint32_t counter = 0;
			while (remaining>0)
			{
				auto data = current_chunk->data;
				uint32_t steps = remaining > chunk_size ? chunk_size : remaining;
				remaining -= steps;
				while (steps-- > 0)
					f(*data++);
			}
		}

	private:
		static const uint32_t chunk_size_bit_count = 8;
		static const uint32_t chunk_size = 256;
		static const uint32_t chunk_index_mask = chunk_size - 1;

		struct chunk
		{
			T* data;
			slot** slot_views;
			slot* slots;
		};

		uint32_t m_size = 0;
		uint32_t m_capacity = 0;
		uint32_t m_next;

		vector<chunk> m_chunks;
		memory_resource<size_t>* m_memory;

		void release()
		{
			m_chunks = nullptr;
			m_chunks_size = 0;
			m_chunks_capacity = 0;
			m_size = 0;
			m_capacity = 0;
		}

		void extend()
		{
			if (m_chunks_size == m_chunks_capacity)
			{
				m_chunks_capacity = m_chunks_capacity == 0 ? 1 : m_chunks_capacity << 1;
				m_chunks = reinterpret_cast<chunk*>(std::realloc(m_chunks, sizeof(chunk)*m_chunks_capacity));
			}

			char* memory = reinterpret_cast<char*>(std::malloc((sizeof(T) + sizeof(slot*) + sizeof(slot))*chunk_size));
			chunk new_chunk = {
				reinterpret_cast<T*>(memory),
				reinterpret_cast<slot**>(memory + sizeof(T)*chunk_size),
				reinterpret_cast<slot*>(memory + (sizeof(slot*) + sizeof(T))*chunk_size)
			};

			m_chunks[m_chunks_size] = new_chunk;
			++m_chunks_size;

			m_next = m_size;

			slot* id = new_chunk.slots;

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