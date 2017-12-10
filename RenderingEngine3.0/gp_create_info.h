#pragma once

#include "vulkan.h"
#include "enum_gp.h"
#include "enum_rp.h"
#include "enum_dsl_type.h"

namespace rcq
{
	template<uint32_t gp_type>
	struct gp_create_info;
}