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

		queue(memory_resource<size_t>* memory) :
			m_memory(memory),
			m_last_node(nullptr),
			m_first_node(nullptr),
			m_first_available_node(nullptr),
			m_prep_node(nullptr),
			m_size(0)
		{}

		queue(const queue& other) :
			m_memory(other.m_memory),
			m_first_available_node(nullptr)
		{
			copy(other);
		}

		queue(queue&& other) :
			m_memory(other.m_memory),
			m_first_node(other.m_first_node),
			m_last_node(other.m_last_node),
			m_first_available_node(other.m_first_available_node),
			m_size(other.m_size),
			m_back_node(other.m_back_node)
		{
			other.release();
		}

		~queue()
		{
			reset();
		}

		queue& operator=(const queue& other)
		{
			clear();
			copy(other);
		}

		queue& operator(queue&& other)
		{
			reset();

			m_memory = other.m_memory;
			m_first_node = other.m_first_node;
			m_last_node = other.m_last_node;
			m_first_available_node = other.m_first_available_node;
			m_size = other.m_size;
			m_back_node = other.m_back_node;

			other.release();
		}

		void reset()
		{
			m_size = 0;
			while (m_first_node != nullptr)
			{
				m_memoy->dealloc(reinterpret_cast<size_t>(m_first_node));
				m_first_node = m_first_node->next;
			}
			m_last_node = nullptr;
			while (m_first_available_node != nullptr)
			{
				m_memory->dealloc(reinterpret_cast<size_t>(m_first_available_node));
				m_first_available_node = m_first_available_node->next;
			}
			if (m_back_node != nullptr)
			{
				m_memory->dealloc(reinterpret_cast<size_t>(m_back_node));
				m_back_node = nullptr;
			}
		}

		void clear()
		{
			if (m_size != 0)
			{
				m_size = 0;
				m_last_node->next = m_first_available_node;
				m_first_available_node = m_first_node;
				m_first_node = nullptr;
				m_last_node = nullptr;
			}
			if (m_back_node != nullptr)
			{
				m_back_node->next = m_first_available_node;
				m_first_available_node = m_back_node;
				m_back_node = nullptr;
			}
		}



		T& create_back()
		{
			m_back_node = get_node();
			m_back_node->next = nullptr;
			return m_back_node->data;
		}

		T& back()
		{
			return m_back_node->data;
		}

		void push()
		{
			if (m_size == 0)
			{
				m_first_node = m_back_node;
				m_last_node = m_back_node;
				m_size = 1;
			}
			else
			{
				m_last_node->next = m_back_node;
				++m_size;
			}
		}

		T& front()
		{
			return m_first_node->data;
		}

		void pop()
		{
			node* popped = m_first_node;
			m_first_node = m_first_node->next;
			popped->next = m_first_available_node;
			m_first_available_node = popped;
			--m_size;
		}

	private:
		node* m_last_node;
		node* m_first_node;
		node* m_first_available_node;
		node* m_back_node;
		size_t m_size;
		memory_resouce<size_t>* m_memory;

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

		void release()
		{
			m_last_node = nullptr;
			m_first_node = nullptr;
			m_first_available_node = nullptr;
			m_back_node = nullptr;
			m_size = 0;
		}

		void copy(const queue& other)
		{
			m_size = other.m_size;
			if (m_size != 0)
			{
				m_first_node = get_node();
				m_last_node = m_first_node;
				node* n = other.m_first_node;
				memcpy(&m_last_node->data, &n->data, sizeof(T));
				n = n->next;
				while (n != nullptr)
				{
					m_last_node->next = get_node();
					m_last_node = m_last_node->next;
					memcpy(&m_last_node->data, &n->data, sizeof(T));
					n = n->next;
				}
				m_last_node->next = nullptr;
			}
			else
			{
				m_last_node = nullptr;
				m_first_node = nullptr;
			}

			if (other.m_back_node != nullptr)
			{
				m_back_node = get_node();
				m_back_node->next = nullptr;
				memcpy(&m_back_node->data, &other.m_back_node->data, sizeof(T));
			}
			else
			{
				m_back_node = nullptr;
			}
		}
	};
	
} //namespace rcq