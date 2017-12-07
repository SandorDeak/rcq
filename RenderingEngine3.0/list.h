#pragma once

#include "memory_resource.h"
#include "iterator.h"

namespace rcq
{
	template<typename T>
	class list
	{
	public:

		struct node : basic_node
		{
			typename T data;
		};
 
		struct iterator
		{
			iterator(basic_node* p) : ptr(p) {}

			iterator& operator++()
			{
				ptr = ptr->next;
				return *this;
			}
			iterator& operator--()
			{
				ptr = ptr->prev;
				return *this;
			}
			 T& operator* ()
			{
				return static_cast<node*>(ptr)->data;
			}
			T* operator->()
			{
				return &(static_cast<node*>(ptr)->data);
			}
			bool operator==(iterator other) const
			{
				return other.ptr == ptr;
			}
			bool operator!=(iterator other) const
			{
				return other.ptr != ptr;
			}
			node* node_ptr()
			{
				return static_cast<node*>(ptr);
			}
		private:
			basic_node* ptr;
		};

		struct reverse_iterator
		{
			reverse_iterator(basic_node* p) : ptr(p) {}

			reverse_iterator& operator++()
			{
				ptr = ptr->prev;
				return *this;
			}
			reverse_iterator& operator--()
			{
				ptr = ptr->next;
				return *this;
			}
			T& operator* ()
			{
				return static_cast<node*>(ptr)->data;
			}
			T* operator->()
			{
				return &(static_cast<node*>(ptr)->data);
			}
			bool operator==(reverse_iterator other) const
			{
				return other.ptr == ptr;
			}
			bool operator!=(reverse_iterator other) const
			{
				return other.ptr != ptr;
			}
			node* node_ptr()
			{
				return static_cast<node*>(ptr);
			}
		private:
			basic_node* ptr;
		};

		struct const_iterator
		{
			const_iterator(const basic_node* p) : ptr(p) {}

			const_iterator& operator++()
			{
				ptr = ptr->next;
				return *this;
			}
			reverse_iterator& operator--()
			{
				ptr = ptr->prev;
				return *this;
			}
			const T& operator* ()
			{
				return static_cast<const node*>(ptr)->data;
			}
			const T* operator->()
			{
				return &(static_cast<const node*>(ptr)->data);
			}
			bool operator==(const_iterator other) const
			{
				return other.ptr == ptr;
			}
			bool operator!=(const_iterator other) const
			{
				return other.ptr != ptr;
			}
			const node* node_ptr()
			{
				return static_cast<const node*>(ptr);
			}
		private:
			const basic_node* ptr;
		};

		list() {}

		list(memory_resource* memory) :
			m_memory(memory),
			m_first_available_node(nullptr),
			m_size(0)
		{
			m_rend.next = &m_end;
			m_end.prev = &m_rend;
		}

		void init(memory_resource* memory)
		{
			m_memory = memory;
		}

		list(const list& other) :
			m_memory(other.m_memory),
			m_size(other.m_size),
			m_first_available_node(nullptr)
		{
			node* n = static_cast<node*>(&m_rend);
			node* m = static_cast<node*>(other.m_rend.next);
			while (m != &other.m_end)
			{
				n->next = get_node();
				n->next->prev = n;
				n = static_cast<node*>(n->next);
				memcpy(&n->data, &m->data, sizeof(T));
				m = static_cast<node*>(m->next);
			}
			n->next = &m_end;
			m_end.prev = n;
		}

		list(list&& other) :
			m_memory(other.m_memory),
			m_size(other.m_size),
			m_first_available_node(other.m_first_available_node)
		{
			if (m_size == 0)
			{
				m_end.prev = &m_rend;
				m_rend.next = &m_end;
			}
			else
			{
				m_end.prev = other.m_end.prev;
				m_rend.next = other.m_rend.next;

				m_end.prev->next = &m_end;
				m_rend.next->prev = &m_rend;

				other.m_end.prev = &other.m_rend;
				other.m_rend.next = &other.m_end;
				other.m_size = 0;
			}
				
			other.m_first_available_node = nullptr;
		}

		list& operator=(const list& other)
		{
			if (m_size != 0)
			{
				m_end.prev->next = m_first_available_node;
				m_first_available_node = static_cast<node*>(m_rend.next);
			}
			m_size = other.m_size;

			node* n = static_cast<node*>(&m_rend);
			node* m = static_cast<node*>(other.m_rend.next);
			while (m != &other.m_end)
			{
				n->next = get_node();
				n->next->prev = n;
				n = static_cast<node*>(n->next);
				memcpy(&n->data, &m->data, sizeof(T));
				m = static_cast<node*>(m->next);
			}
			n->next = &m_end;
			m_end.prev = n;
			return *this;
		}

		list& operator=(list&& other)
		{
			while (m_rend.next != &m_end)
			{
				basic_node* next = m_rend.next->next;
				m_memory->deallocate(reinterpret_cast<size_t>(m_rend.next));
				m_rend.next = next;
			}
			m_end.prev = &m_rend;

			while (m_first_available_node != nullptr)
			{
				node* next = static_cast<node*>(m_first_available_node->next);
				m_memory->deallocate(reinterpret_cast<size_t>(m_first_available_node));
				m_first_available_node = next;
			}

			m_memory = other.m_memory;
			m_size = other.m_size;
			m_first_available_node = other.m_first_available_node;
			if (m_size != 0)
			{
				m_rend.next = other.m_rend.next;
				m_end.prev = other.m_end.prev;
				m_rend.next->prev = &m_rend;
				m_end.prev->next = &m_end;

				other.m_size = 0;
				other.m_rend.next = &other.m_end;
				other.m_end.prev = &other.m_rend;
			}
			
			

			return *this;
		}

		~list()
		{
			while (m_rend.next != &m_end)
			{
				basic_node* next = m_rend.next->next;
				m_memory->deallocate(reinterpret_cast<size_t>(m_rend.next));
				m_rend.next = next;
			}

			while (m_first_available_node != nullptr)
			{
				node* next = static_cast<node*>(m_first_available_node->next);
				m_memory->deallocate(reinterpret_cast<size_t>(m_first_available_node));
				m_first_available_node = next;
			}
		}

		void reset()
		{
			while(m_rend.next!=&m_end)
			{
				basic_node* next = m_rend.next->next;
				m_memory->deallocate(reinterpret_cast<size_t>(m_rend.next));
				m_rend.next = next;
			}
			m_end.prev = &m_rend;

			while (m_first_available_node != nullptr)
			{
				node* next = static_cast<node*>(m_first_available_node->next);
				m_memory->deallocate(reinterpret_cast<size_t>(m_first_available_node));
				m_first_available_node = next;
			}
			m_size = 0;
		}

		void clear()
		{
			if (m_size > 0)
			{
				m_end.prev->next = m_first_available_node;
				m_first_available_node = static_cast<node*>(m_rend.next);
				m_end.prev = &m_rend;
				m_rend.next = &m_end;
				m_size = 0;
			}
		}

		iterator insert(iterator it)
		{
			node* new_node = get_node();
			insert_node(it, new_node);
			return new_node;
		}
		void erase(iterator it)
		{
			node* n = extract_node(it);
			n->next = m_first_available_node;
			m_first_available_node = n;
		}
		reverse_iterator insert(reverse_iterator it)
		{
			node* new_node = get_node();
			insert_node(it, new_node);
			return new_node;
		}
		void erase(reverse_iterator it)
		{
			node* n = extract_node(it);
			n->next = m_first_available_node;
			m_first_available_node = n;
		}

		iterator begin()
		{
			return m_rend.next;
		}
		iterator end()
		{
			return &m_end;
		}
		reverse_iterator rbegin()
		{
			return m_end.prev;
		}
		reverse_iterator rend()
		{
			return &m_rend;
		}
		const_iterator cbegin() const
		{
			return m_rend.next;
		}
		const_iterator cend() const
		{
			return &m_end;
		}

		size_t size() const
		{
			return m_size;
		}

		node* extract_node(iterator it)
		{
			it.node_ptr()->prev->next = it.node_ptr()->next;
			it.node_ptr()->next->prev = it.node_ptr()->prev;
			--m_size;
			return it.node_ptr();
		}
		node* extract_node(reverse_iterator it)
		{
			it.node_ptr()->prev->next = it.node_ptr()->next;
			it.node_ptr()->next->prev = it.node_ptr()->prev;
			--m_size;
			return it.node_ptr();
		}
		iterator insert_node(iterator it, node* n)
		{
			n->next = it.node_ptr();
			n->prev = it.node_ptr()->prev;
			n->prev->next = n;
			n->next->prev = n;
			++m_size;
			return n;
		}
		reverse_iterator insert_node(reverse_iterator it, node* n)
		{
			n->next = it.node_ptr()->next;
			n->prev = it.node_ptr();
			n->next->prev = n;
			n->prev->next = n;
			++m_size;
			return n;
		}

	private:
		basic_node m_rend;
		basic_node m_end;
		size_t m_size;
		node* m_first_available_node;
		memory_resource* m_memory;

		node* get_node()
		{
			if (m_first_available_node == nullptr)
			{
				return reinterpret_cast<node*>(m_memory->allocate(sizeof(node), alignof(node)));
			}
			node* ret = m_first_available_node;
			m_first_available_node = static_cast<node*>(m_first_available_node->next);
			return ret;
		}
	};
}