#pragma once

#include "vulkan.h"

namespace rcq
{
	struct res_image
	{
		VkImage image;
		VkImageView view;
	};
}