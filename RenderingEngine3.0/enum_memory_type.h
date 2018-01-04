#pragma once

#include <stdint.h>

namespace rcq
{
	enum MEMORY_TYPE : uint32_t //for Geforce GTX950m 
	{
		MEMORY_TYPE_DL0=7, //device local, mainly for buffers
		MEMORY_TYPE_DL1=8, //device local, mainly for images
		MEMORY_TYPE_HVC=9, //host visible, coherent
		MEMORY_TYPE_HVCC=10 //host visible, coherent, cached
	};
}