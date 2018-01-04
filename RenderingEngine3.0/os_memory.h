#pragma once

#include <malloc.h>
#include "host_memory.h"

namespace rcq
{
	class os_memory : public host_memory
	{
	public:
		os_memory() :
			host_memory(~0, nullptr)
		{}

		size_t allocate(size_t size, size_t alignment) override
		{
			return reinterpret_cast<size_t>(_aligned_malloc(size, alignment));
		}

		void deallocate(size_t p) override
		{
			_aligned_free(reinterpret_cast<void*>(p));
		}

	};

	static os_memory OS_MEMORY;
}
