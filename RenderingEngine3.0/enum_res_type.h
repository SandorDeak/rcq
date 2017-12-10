#pragma once

#include <stdint.h>

namespace rcq
{
	enum RES_TYPE : uint32_t
	{
		RES_TYPE_MAT_OPAQUE,
		//RES_TYPE_MAT_EM,
		RES_TYPE_SKY,
		RES_TYPE_TERRAIN,
		RES_TYPE_WATER,
		RES_TYPE_MESH,
		RES_TYPE_TR,
		RES_TYPE_COUNT
	};
}