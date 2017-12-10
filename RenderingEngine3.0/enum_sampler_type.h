#pragma once

#include <stdint.h>

namespace rcq
{
	enum SAMPLER_TYPE : uint32_t
	{
		SAMPLER_TYPE_NORMALIZED_COORD,
		SAMPLER_TYPE_UNNORMALIZED_COORD,
		SAMPLER_TYPE_COUNT
	};
}