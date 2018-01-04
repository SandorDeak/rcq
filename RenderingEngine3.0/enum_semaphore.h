#pragma once

#include <stdint.h>

namespace rcq
{
	enum SEMAPHORE : uint32_t
	{
		SEMAPHORE_PREIMAGE_READY,
		SEMAPHORE_BLOOM_READY,
		SEMAPHORE_IMAGE_AVAILABLE,
		SEMAPHORE_RENDER_FINISHED,
		SEMAPHORE_COUNT
	};
}