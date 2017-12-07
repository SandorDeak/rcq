#pragma once

#include "foundation.h"

namespace rcq
{
	class memory_resource
	{
	public:
		memory_resource() {}

		memory_resource(uint64_t max_alignment, memory_resource* upstream) :
			m_upstream(upstream),
			m_max_alignment(max_alignment)
		{}

		virtual ~memory_resource() {}

		virtual uint64_t allocate(uint64_t size, uint64_t alignment) = 0;
		virtual void deallocate(uint64_t p) = 0;

		static uint64_t align(uint64_t p, uint64_t alignment)
		{
			return (p + (alignment - 1)) & (~(alignment - 1));
		}

		uint64_t max_alignment()
		{
			return m_max_alignment;
		}

		memory_resource* upstream() const
		{
			return m_upstream;
		}

	protected:
		void init(uint64_t max_alignment, memory_resource* upstream)
		{
			m_max_alignment = max_alignment;
			m_upstream = upstream;
		}

		memory_resource* m_upstream;
		uint64_t m_max_alignment;
	};
}