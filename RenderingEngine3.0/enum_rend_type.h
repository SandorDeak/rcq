#pragma once

#include <stdint.h>

namespace rcq
{
	enum REND_TYPE : uint32_t
	{
		REND_TYPE_OPAQUE_OBJECT,
		REND_TYPE_SKY,
		REND_TYPE_TERRAIN,
		REND_TYPE_WATER,
		REND_TYPE_COUNT
	};
}

