#pragma once

#include "host_memory.h"

namespace rcq
{
	template<typename T>
	class vector
	{
	public:
		typedef T* iterator;

		vector() {}

		vector(host_memory* memory, size_t size = 0) :
			m_memory(memory),
			m_size(size),
			m_data(nullptr)
		{
			if (m_size != 0)
			{
				m_capacity = 1;
				while (m_capacity < m_size)
				{
					m_capacity <<= 1;
				}
				m_data = reinterpret_cast<T*>(m_memory->allocate(m_capacity * sizeof(T), alignof(T)));
			}
		}

		void init(host_memory* memory, size_t size = 0)
		{
			m_memory = memory;
			m_size = size;
			m_data = nullptr;

			if (m_size != 0)
			{
				m_capacity = 1;
				while (m_capacity < m_size)
				{
					m_capacity <<= 1;
				}
				m_data = reinterpret_cast<T*>(m_memory->allocate(m_capacity * sizeof(T), alignof(T)));
			}
		}

		~vector()
		{
			reset();
		}

		void reset()
		{
			m_size = 0;
			m_capacity = 0;
			if (m_data != nullptr)
			{
				m_memory->deallocate(reinterpret_cast<size_t>(m_data));
				m_data = 0;
			}
		}

		void clear()
		{
			m_data = nullptr;
			m_size = 0;
		}

		host_memory* memory_resource()
		{
			return m_memory;
		}

		vector(const vector& other) :
			m_memory(other.m_memory),
			m_size(other.m_size),
			m_capacity(other.m_capacity),
			m_data(nullptr)
		{
			if (m_size != 0)
			{

				m_data = reinterpret_cast<T*>(m_memory->allocate(m_capacity * sizeof(T), alignof(T)));
				memcpy(m_data, other.m_data, m_size * sizeof(T));
			}
		}

		vector(vector&& other) :
			m_memory(other.m_memory),
			m_size(other.m_size),
			m_capacity(other.m_capacity),
			m_data(other.m_data)
		{
			other.release();
		}

		vector& operator=(const vector& other)
		{
			m_size = other.m_size;
			if (m_size > m_capacity)
			{
				m_memory->deallocate(reinterpret_cast<uint64_t>(m_data));
				m_capacity = other.m_capacity;
				m_data = reinterpret_cast<T*>(m_memory->allocate(m_capacity * sizeof(T), alignof(T)));
			}
			memcpy(m_data, other.m_data, m_size * sizeof(T));
			return *this;
		}

		vector& operator=(vector&& other)
		{
			reset();

			m_memory = other.m_memory;
			m_capacity = other.m_capacity;
			m_size = other.m_size;
			m_data = other.m_data;

			other.release();
		}

		bool empty()
		{
			return m_size == 0;
		}

		T* data()
		{
			return m_data;
		}

		T& operator[](size_t i)
		{
			return *(m_data + i);
		}

		iterator push_back()
		{
			if (m_size == m_capacity)
			{

				m_capacity = m_capacity == 0 ? 1 : m_capacity << 1;
				T* new_data = reinterpret_cast<T*>(m_memory->allocate(m_capacity * sizeof(T), alignof(T)));
				if (m_size != 0)
				{
					memcpy(new_data, m_data, m_size * sizeof(T));
					m_memory->deallocate(reinterpret_cast<uint64_t>(m_data));
				}
				m_data = new_data;
			}

			++m_size;
			return m_data + m_size-1;
		}

		iterator begin()
		{
			return m_data;
		}

		iterator end()
		{
			return m_data + m_size;
		}

		iterator last()
		{
			return m_data + m_size - 1;
		}

		size_t size()
		{
			return m_size;
		}

		void resize(size_t size)
		{
			if (size <= m_capacity)
			{
				m_size = size;
			}
			else
			{
				while (m_capacity < size)
					m_capacity <<= 1;

				m_memory->deallocate(reinterpret_cast<size_t>(m_data));
				m_memory->allocate(m_capacity * sizeof(T), alignof(T));
				m_size = size;
			}
		}

	private:
		host_memory* m_memory;
		T* m_data;
		size_t m_size;
		size_t m_capacity;

		void release()
		{
			m_size = 0;
			m_capacity = 0;
			m_data = nullptr;
		}
	};
}