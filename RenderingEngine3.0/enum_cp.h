#pragma once

#include <stdint.h>

namespace rcq
{
	enum CP : uint32_t
	{
		CP_TERRAIN_TILE_REQUEST,
		CP_WATER_FFT,
		CP_BLOOM,
		CP_COUNT
	};
}