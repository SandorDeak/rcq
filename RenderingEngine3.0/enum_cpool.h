#pragma once

#include <stdint.h>

namespace rcq
{
	enum CPOOL : uint32_t
	{
		CPOOL_GRAPHICS,
		CPOOL_PRESENT,
		CPOOL_COMPUTE,
		CPOOL_COUNT
	};
}