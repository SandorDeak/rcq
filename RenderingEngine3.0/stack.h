#pragma once

#include "memory_resource.h"

namespace rcq
{
	template<typename T>
	class stack
	{
	public:

		stack(memory_resource* memory) :
			m_memory(memory),
			m_size(0),
			m_top_node(nullptr),
			m_first_available_node(nullptr)
		{}

		stack(const stack& other) :
			m_memory(other.m_memory),
			m_size(other.m_size),
			m_first_available_node(nullptr)
		{
			copy(other);
		}

		stack(stack&& other) :
			m_memory(other.m_memory),
			m_size(other.m_size),
			m_first_available_node(other.m_first_available_node),
			m_top_node(other.m_top_node)
		{
			other.release();
		}


		~stack()
		{
			reset();
		}

		stack& operator=(const stack& other)
		{
			clear();
			copy(other);
			return *this;
		}

		stack& operator=(stack&& other)
		{
			reset();

			m_memory = other.m_memory;
			m_size = other.m_size;
			m_first_available_node = other.m_first_available_node;
			m_top_node = other.m_top_node;

			other.release();
		}

		void reset()
		{
			while (m_top_node != nullptr)
			{
				m_memory->deallocate(reinterpret_cast<size_t>(m_top_node));
				m_top_node = m_top_node->next;
			}
			while (m_first_available_node != nullptr)
			{
				m_memory->deallocate(reinterpret_cast<size_t>(m_first_available_node));
				m_first_available_node = m_first_available_node->next;
			}
			m_size = 0;
		}

		bool empty()
		{
			return m_top_node == nullptr;
		}

		void clear()
		{
			node* n = nullptr;
			node* m = m_top_node;
			while (m != nullptr)
			{
				n = m;
				m = m->next;
			}

			if (n != nullptr)
			{
				n->next = m_first_available_node;
				m_first_available_node = m_top_node;
				m_top_node = nullptr;
				m_size = 0;
			}
		}

		T* push()
		{
			node* new_node = get_node();
			new_node->next = m_top_node;
			m_top_node = new_node;
			++m_size;
			return &new_node->data;
		}

		T* top()
		{
			return &m_top_node->data;
		}

		void pop()
		{
			node* popped = m_top_node;
			m_top_node = m_top_node->next;
			popped->next = m_first_available_node;
			m_first_available_node = popped;
			--m_size;
		}
	private:
		struct node
		{
			node* next;
			T data;
		};

		node* m_top_node;
		node* m_first_available_node;
		size_t m_size;
		memory_resource* m_memory;

		node* get_node()
		{
			if (m_first_available_node != nullptr)
			{
				node* ret = m_first_available_node;
				m_first_available_node = m_first_available_node->next;
				return ret;
			}
			return reinterpret_cast<node*>(m_memory->allocate(sizeof(T), alignof(T)));
		}

		void copy(const stack& other)
		{
			if (other.m_size != 0)
			{
				node* n = other.m_top_node;
				m_top_node = get_node();
				node* m = m_top_node;
				memcpy(&m->data, &n->data, sizeof(T));
				n = n->next;

				while (n != nullptr)
				{
					m->next = get_node();
					m = m->next;
					memcpy(&m->data, &n->data, sizeof(T));
					n = n->next;
				}
				m->next = nullptr;
			}
			else
			{
				m_top_node = nullptr;
			}
			m_size = other.m_size;
		}

		void release()
		{
			m_size = 0;
			m_first_available_node = nullptr;
			m_top_node = nullptr;
		}
	};
}