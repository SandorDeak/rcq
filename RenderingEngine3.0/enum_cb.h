#pragma once

#include <stdint.h>

namespace rcq
{
	enum CB : uint32_t
	{
		CB_RENDER,
		CB_TERRAIN_REQUEST,
		CB_WATER_FFT,
		CB_BLOOM,
		CB_COUNT
	};
}