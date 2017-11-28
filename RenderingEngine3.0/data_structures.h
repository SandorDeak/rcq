#pragma once

#include "memory_resource.h"

namespace rcq
{
	template<typename T>
	class vector
	{
		typedef T* iterator;

		vector(memory_resource<size_t>* memory, size_t size = 0) :
			m_memory(memory),
			m_size(size),
			m_capacity(0)
			m_data(nullptr)
		{
			if (m_size != 0)
			{
				m_capacity = 1;
				while (m_capacity < m_size)
				{
					m_capacity <<= 1;
				}
				m_data=reinterpret_cast<T*>(m_memory->allocate(m_capacity * sizeof(T), alignof(T)));
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

		memory_resource<size_t>* memory_resource()
		{
			return m_memory;
		}

		vector(const vector& other):
			m_memory(other.m_memory),
			m_size(other.m_size),
			m_capacity(other.m_capacity),
			m_data(nullptr)
		{
			if (m_size != 0)
			{

				m_data=reinterpret_cast<T*>(m_memory->allocate(m_capacity * sizeof(T), alignof(T)));
				memcpy(m_data, other.m_data, m_size * sizeof(T));
			}
		}

		vector(vector&& other):
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
			if(m_size>m_capacity)
			{
				m_memory->deallocate(reinterpret_cast<size_t>(m_data));
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

		T& push_back()
		{
			if (m_size == m_capacity)
			{

				m_capacity *= 2;
				T* new_data = reinterpret_cast<T*>(m_memory->allocate(m_capacity * sizeof(T), alignof(T)));
				memcpy(new_data, m_data, m_size * sizeof(T));
				m_memory->deallocate(reinterpret_cast<size_t>(m_data));
				m_data = new_data;
			}

			return *(m_data + m_size++);
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

		
	private:
		T* m_data;
		size_t m_size;
		size_t m_capacity;
		memory_resource<size_t>* m_memory;

		void release()
		{
			m_size = 0;
			m_capacity = 0;
			m_data = nullptr;
		}
	};

	template<typename T>
	class list
	{
	public:
		struct node
		{
			node* prev;
			node* next;
			T data;
		};

		struct iterator : public node*
		{
			iterator& operator++()
			{
				*this = (*this)->next;
				return *this;
			}
			iterator& operator--()
			{
				*this = (*this)->prev;
				return *this;
			}
			operator T*()
			{
				return &((*this)->data);
			}
		};

		list(memory_resource<size_t>* memory) :
			m_memory(memory),
			m_first_available_node(nullptr),
			m_first_node(nullptr),
			m_last_node(nullptr),
			m_size(0)
		{}

		list(const list& other) :
			m_memory(other.m_memory),
			m_size(other.m_size),
			m_first_available_node(nullptr)
		{
			copy(other);
		}

		list(list&& other) :
			m_memory(other.m_memory),
			m_size(other.m_size),
			m_first_available_node(other.m_first_available_node),
			m_first_node(other.m_first_node),
			m_last_node(other.m_last_node)
		{
			other.release();
		}

		list& operator=(const list& other)
		{
			clear();
			copy(other);
			return *this;
		}

		list& operator=(list&& other)
		{
			reset();

			m_memory = other.m_memory;
			m_size = other.m_size;
			m_first_available_node = other.m_first_available_node;
			m_first_node = other.m_first_node;
			m_last_node = other.m_last_node;

			other.release();
		}

		~list()
		{
			reset();
		}

		void reset()
		{
			while (m_first_node != nullptr)
				m_memory->deallocate(reinterpret_cast<size_t>(m_first_node));
			while (m_first_available_node!=nullptr)
				m_memory->deallocate(reinterpret_cast<size_t>(m_first_available_node));
			m_size = 0;
		}

		void clear()
		{
			m_last_node->next = m_first_available_node;
			m_first_available_node = m_first_node;
			m_first_node = nullptr;
			m_last_node = nullptr;
			m_size = 0;
		}

		T& push_back()
		{
			node* new_node= get_node();

			if (m_size == 0)
			{
				m_last_node = new_node;
				m_first_node = new_node;
				new_node->prev = nullptr;
				new_node->next = nullptr;
			}
			else
			{
				new_node->prev = m_last_node;
				m_last_node->next = new_node;
				m_last_node = new_node;
				m_last_node->next = nullptr;
			}
			++m_size;
			return m_last_node->data;
		}

		T& push_front()
		{
			node* new_node = get_node();

			if (m_size == 0)
			{
				m_last_node = new_node;
				m_first_node = new_node;
				new_node->next = nullptr;
				new_node->prev = nullptr;
			}
			else
			{
				new_node->next = m_first_node;
				new_node->prev = nullptr;
				m_first_node->prev = new_node;
				m_first_node = new_node;
			}

			++m_size;
			return m_first_node->data;
		}

		T& insert(iterator it)
		{
			if (it == m_last_node)
				return push_back();

			node* new_node= get_node();
			new_node->prev = it;
			new_node->next = it->next;
			it->next->prev = new_node;
			it->next = new_node;
			++m_size;
			return new_node->data;
		}

		void pop_back()
		{
			m_last_node->next = m_first_available_node;
			m_first_available_node = m_last_node;
			m_last_node = m_last_node->prev;
			if (m_size == 1)
				m_first_node = nullptr;;
			else
				m_last_node->next = nullptr;

			--m_size;
		}

		void pop_front()
		{
			if (m_size == 1)
			{
				m_first_node->next = m_first_available_node;
				m_first_available_node = m_first_node;
				m_first_node = nullptr;
				m_last_node = nullptr;
			}
			else
			{
				m_first_node = m_first_node->next;
				m_first_node->prev->next = m_first_available_node;
				m_first_available_node = m_first_node->prev;
				m_first_node->prev = nullptr;
			}
			--m_size;
		}

		void erase(iterator it)
		{
			if (it == m_first_node)
				pop_front();
			else if (it == m_last_node)
				pop_back();
			else
			{
				it->prev->next = it->next;
				it->next->prev = it->prev;
				it->next = m_first_available_node;
				m_first_available_node = it;
				--m_size;
			}
		}

		iterator begin()
		{
			return m_first_node;
		}
		iterator end()
		{
			return nullptr;
		}
		iterator last()
		{
			return m_last_node;
		}


	private:
		node*  m_first_node;
		node* m_last_node;
		size_t m_size;
		node* m_first_available_node;
		memory_resource<size_t>* m_memory;

		node* get_node()
		{
			if (m_first_available_node == nullptr)
			{
				return reinterpret_cast<node*>(m_memory->allocate(sizeof(node), alignof(node)));
			}
			node* ret = m_first_available_node;
			m_first_available_node = m_first_available_node->next;
			return ret;
		}

		void copy(const list& other)
		{
			if (other.m_size != 0)
			{
				m_first_node = get_node();
				m_first_node->prev = nullptr;
				m_last_node = m_first_node;		
				node* n = other.m_first_node;
				memcpy(&m_last_node->data, &n->data, sizeof(T));
				n = n->next;
				while(n!=nullptr)
				{
					m_last_node->next = get_node();
					m_last_node->next->prev = m_last_node;
					m_last_node = m_last_node->next;
					memcpy(&m_last_node->data, &n->data, sizeof(T));
					n = n->next;
				}
				m_last_node->next = nullptr;
			}
			else
			{
				m_first_node = nullptr;
				m_last_node = nullptr;
			}
			m_size = other.m_size;
		}

		void release()
		{
			m_size = 0;
			m_first_available_node = nullptr;
			m_first_node = nullptr;
			m_last_node = nullptr;
		}
	};

	template<typename T>
	class stack
	{
	public:

		struct node
		{
			node* next;
			T data;
		};

		stack(memory_resource<size_t>* memory) :
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
				m_memory->deallocate(reinterpret_cast<size_t>(m_top_node++));
			while (m_first_available_node != nullptr)
				m_memory->deallocate(reinterpret_cast<size_t>(m_first_available_node++));
			m_size = 0;
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

		T& push()
		{
			node* new_node = get_node();
			new_node->next = m_top_node;
			m_top_node = new_node;
			++m_size;
			return new_node->data;
		}

		T& top()
		{
			return m_top_node->data;
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
		node* m_top_node;
		node* m_first_available_node;
		size_t m_size;
		memory_resource<size_t>* m_memory;

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

	template<typename T>
	class queue
	{
	public:
		struct node
		{
			node* next;
			T data;
		};

		

	private:
		node* m_last_node;
		node* m_first_node;
		node* m_first_available_node;
		size_t m_size;
	};
	
} //namespace rcq