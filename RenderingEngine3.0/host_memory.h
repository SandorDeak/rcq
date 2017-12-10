#pragma once

namespace rcq
{
	class host_memory
	{
	public:
		host_memory() {}

		host_memory(size_t max_alignment, host_memory* upstream) :
			m_upstream(upstream),
			m_max_alignment(max_alignment)
		{}

		virtual ~host_memory() {}

		virtual size_t allocate(size_t size, size_t alignment) = 0;
		virtual void deallocate(size_t p) = 0;

		static size_t align(size_t p, size_t alignment)
		{
			return (p + (alignment - 1)) & (~(alignment - 1));
		}

		size_t max_alignment()
		{
			return m_max_alignment;
		}

		host_memory* upstream() const
		{
			return m_upstream;
		}

	protected:
		void init(size_t max_alignment, host_memory* upstream)
		{
			m_max_alignment = max_alignment;
			m_upstream = upstream;
		}

		host_memory* m_upstream;
		size_t m_max_alignment;
	};
}