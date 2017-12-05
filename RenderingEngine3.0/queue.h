#pragma once

#include "memory_resource.h"

namespace rcq
{
	template<typename T>
	class queue
	{
	public:
		struct node
		{
			node* next;
			T data;
		};

		queue(memory_resource* memory) :
			m_memory(memory),
			m_size(0)
		{}

		queue(const queue& other) = delete;

		queue(queue&& other) :
			m_memory(other.m_memory),
			m_first(other.m_first),
			m_first_back(other.m_first_back),
			m_first_free(other.m_first_free),
			m_size(other.m_size)
		{
			other.m_size.store(0, std::memory_order_relaxed);
			other.m_first.store(nullptr, std::memory_order_relaxed);
		}

		~queue()
		{
			node* first = m_first.load(std::memory_order_acquire);
			if (first != nullptr)
			{
				node* end = first;
				first = first->next;
				end->next = nullptr;
				while (first != nullptr)
				{
					node* next = first->next;
					m_memory->deallocate(reinterpret_cast<uint64_t>(first));
					first = next;
				}
			}
		}
			
		queue& operator=(const queue& other) = delete;

		queue& operator=(queue&& other)
		{
			node* first = m_first.load(std::memory_order_acquire);
			if (first != nullptr)
			{
				node* end = first;
				first = first->next;
				end->next = nullptr;
				while (first != nullptr)
				{
					node* next = first->next;
					m_memory->deallocate(reinterpret_cast<uint64_t>(first));
					first = next;
				}
			}

			m_memory = other.m_memory;
			m_first = other.m_first;
			m_first_back = other.m_first_back;
			m_first_free = other.m_first_free;
			m_size = other.m_size;

			other.m_size = 0;
			other.m_size_back = 0;
			other.m_frist = nullptr;	

			return *this;
		}

		void init()
		{
			m_first = reinterpret_cast<node*>(m_memory->allocate(sizeof(node), alignof(node)));
			m_first->next = m_first;
			m_first_back = m_first;
			m_first_free = m_first;
		}

		void reset()
		{
			m_size = 0;
			m_size_back = 0;

			node* first = m_first.load(std::memory_order_acquire);
			if (first != nullptr)
			{
				node* end = first;
				first = first->next;
				end->next = nullptr;
				while (first != nullptr)
				{
					node* next = first->next;
					m_memory->deallocate(reinterpret_cast<uint64_t>(first));
					first = next;
				}

				m_first.store(nullptr, std::memory_order_release);
			}
		}

		void clear()
		{
			node* first_free = m_first_free.load(std::memory_order_relaxed);
			m_first.store(first_free, std::memory_order_relaxed);
			m_first_back.store(first_free, std::memory_order_relaxed);
			m_size.store(0, std::memory_order_relaxed);
			m_size_back = 0;
		}



		T* create_back()
		{
			
			if (m_first_free!=m_first)
			{
				T* ret = &m_first_free->data;
				m_first_free = m_first_free->next;
				return ret;
			}
			else
			{
				node* last = m_first_back;
				while (last->next != m_first_free)
					last = last->next;

				last->next = reinterpret_cast<node*>(m_memory->allocate(sizeof(node), alignof(node)));
				last->next->next = m_first_free;
				return &last->next->data;
			}
		}

		void commit()
		{
			m_first_back.store(m_first_free, std::memory_order_relaxed;
		}

		T* front()
		{
			return &m_first->data;
		}

		void pop()
		{
			m_first = m_first->next;
		}

		bool empty()
		{
			return m_first == m_first_back;
		}

	private:
		std::atomic<node*> m_first;
		std::atomic<node*> m_first_back;

		node* m_first_free;

		memory_resouce* m_memory;
	};
	
} //namespace rcq