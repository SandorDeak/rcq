#pragma once

#include <stdint.h>

namespace rcq
{
	enum FENCE : uint32_t
	{
		FENCE_RENDER_FINISHED,
		FENCE_COMPUTE_FINISHED,
		FENCE_COUNT
	};
}