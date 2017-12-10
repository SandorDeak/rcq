#pragma once

#include <stdint.h>

namespace rcq
{
	enum TEX_TYPE_FLAG : uint32_t
	{
		TEX_TYPE_FLAG_COLOR = 1,
		TEX_TYPE_FLAG_ROUGHNESS = 2,
		TEX_TYPE_FLAG_METAL = 4,
		TEX_TYPE_FLAG_NORMAL = 8,
		TEX_TYPE_FLAG_HEIGHT = 16,
		TEX_TYPE_FLAG_AO = 32
	};
}
