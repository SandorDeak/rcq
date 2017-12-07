#pragma once

#include "memory_resource.h"
#include <malloc.h>
#include <limits>

namespace rcq
{
	class malloc_free_resource : public memory_resource
	{
	public:
		malloc_free_resource() :
			memory_resource(std::numeric_limits<size_t>::max(), nullptr)
		{}

		size_t allocate(size_t size, size_t alignment)
		{
			return reinterpret_cast<size_t>(_aligned_malloc(size, alignment));
		}

		void deallocate(size_t p)
		{
			_aligned_free(reinterpret_cast<void*>(p));
		}
	};
}
