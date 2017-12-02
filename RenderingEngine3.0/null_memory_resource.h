#pragma once

#include <stdexcept>

#include "memory_resource.h"

namespace rcq
{
	template<typename PType>
	class null_memory_resource : public memory_resource<PType>
	{
	public:
		null_memory_resource(): memory_resource(nullptr, 0) {}

		PType allocate(PType size, PType alignment) override
		{
			throw std::runtime_error("out of memory!");
		}

		void deallocate(PType p) override {}
	};
}