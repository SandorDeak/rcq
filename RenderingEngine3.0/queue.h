#pragma once

#include "host_memory.h"

#include <atomic>

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

		queue() {}

		queue(host_memory* memory) :
			m_memory(memory)
		{}

		void init(host_memory* memory)
		{
			m_memory = memory;
		}

		queue(const queue& other) = delete;

		queue(queue&& other) :
			m_memory(other.m_memory),
			m_first(other.m_first),
			m_first_back(other.m_first_back),
			m_first_free(other.m_first_free)
		{
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
					m_memory->deallocate(reinterpret_cast<size_t>(first));
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
					m_memory->deallocate(reinterpret_cast<size_t>(first));
					first = next;
				}
			}

			m_memory = other.m_memory;
			m_first = other.m_first;
			m_first_back = other.m_first_back;
			m_first_free = other.m_first_free;

			other.m_frist = nullptr;	

			return *this;
		}

		void init_buffer()
		{
			auto n = reinterpret_cast<node*>(m_memory->allocate(sizeof(node), alignof(node)));
			auto m = reinterpret_cast<node*>(m_memory->allocate(sizeof(node), alignof(node)));
			n->next = m;
			m->next = n;	
			m_first_free = n;
			m_first.store(n);
			m_first_back.store(n);
		}

		void reset()
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

				m_first.store(nullptr, std::memory_order_release);
			}
		}

		void clear()
		{
			node* first_free = m_first_free.load(std::memory_order_relaxed);
			m_first.store(first_free, std::memory_order_relaxed);
			m_first_back.store(first_free, std::memory_order_relaxed);
		}



		T* create_back()
		{
			
			if (m_first_free->next!=m_first.load())
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
			m_first_back.store(m_first_free, std::memory_order_relaxed);
		}

		T* front()
		{
			return &m_first.load()->data;
		}

		void pop()
		{
			m_first.store(m_first.load()->next);
		}

		bool empty()
		{
			return m_first.load() == m_first_back.load();
		}

	private:
		std::atomic<node*> m_first;
		std::atomic<node*> m_first_back;

		node* m_first_free;

		host_memory* m_memory;
	};	
} //namespace rcq