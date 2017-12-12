#pragma once

#include "resources.h"

namespace rcq
{
	template<uint32_t res_type>
	struct resource_info
	{
		resource<res_type>::build_info* build_info;
		base_resource* resource;
	};
}