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
			for (auto it=m_chunks.begin(); it!=m_chunks.end(); ++it)
			{
				m_memory->deallocate(reinterpret_cast<uint64_t>(it->data));
			}
		}

		slot_map& operator=(slot_map&& other)
		{
			for (auto it = m_chunks.begin(); it != m_chunks.end(); ++it)
			{
				m_memory->deallocate(reinterpret_cast<uint64_t>(it->data));
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


  ////////////////////////////////////////////////////////////////
  /*public:

  slot_map(memory_resource* memory) :
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

  for (auto& c : m_chunks)
  m_memory->deallocate(reinterpret_cast<uint64_t>(c.data));

  m_chunks.reset();
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
  if (m_chunks.size()==0)
  return;

  for (auto& c : m_chunks)
  m_memory->deallocate(reinterpret_cast<uint64_t>(c.data));

  m_chunks.reset();
  }

  /*void swap(slot_map& other)
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
  if (m_chunks.empty())
  return;


  for (auto& c : m_chunks)
  m_memory->deallocate(reinterpret_cast<uint64_t>(c.data));

  m_chunks.reset();

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
  auto current_chunk = m_chunks.data();
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
  memory_resource* m_memory;

  void release()
  {
  m_chunks.reset();
  m_size = 0;
  m_capacity = 0;
  }

  void extend()
  {

  char* memory = reinterpret_cast<char*>(std::malloc((sizeof(T) + sizeof(slot*) + sizeof(slot))*chunk_size));
  chunk new_chunk = {
  reinterpret_cast<T*>(memory),
  reinterpret_cast<slot**>(memory + sizeof(T)*chunk_size),
  reinterpret_cast<slot*>(memory + (sizeof(slot*) + sizeof(T))*chunk_size)
  };

  *m_chunks.push_back() = new_chunk;

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
  }*/