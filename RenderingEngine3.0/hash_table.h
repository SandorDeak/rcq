#pragma once

#include "list.h"
#include "vector.h"

namespace rcq
{
	template<typename Key, typename T> 
	class hash_table
	{
	public:

		hash_table(memory_resouce<size_t>* memory) :
			m_hash(hash),
			m_memory(memory),
			m_buckets(memory, hash_key_count<Key>())
		{
			auto bucket_place = m_buckets.data();
			for (auto it=m_buckets.begin(); it!=m_buckets.end(); ++it)
			{
				new(it) list<record>(m_memory);
			}
		}

		~hash_table()
		{
			for (auto it=m_buckets.begin(); it!=m_buckets.end(); ++it)
			{
				it->~list<record>();
			}
		}

		T* get(Key k)
		{
			auto bucket = m_buckets[hash(k)];
			for (auto it = bucket->begin(); it != bucket->end(); ++it)
			{
				if (it->key == k)
					return &it->data;
			}
			return nullptr;
		}

		bool erase(Key k)
		{
			auto bucket = m_buckets[hash(k)];
			for (auto it = bucket->begin(); it != bucket->end(); ++it)
			{
				if (it->key == k)
				{
					bucket->erase(it);
					return true;
				}
			}
			return false;
		}

		T* push(Key k)
		{
			return m_buckets[hash(k)].push_back();
		}

		list<record>* bucket(size_t index)
		{
			return m_buckets[index];
		}

	private:
		struct record
		{
			Key key;
			T data;
		};

		vector<list<record>> m_buckets;
		size_t m_size;
		memory_resource<size_t>* m_memory;
	};
}