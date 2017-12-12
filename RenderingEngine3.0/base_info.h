#pragma once

#include "vulkan.h"

#include "const_swap_chain_image_count.h"

#include "enum_queue.h"

namespace rcq
{
	struct base_info
	{
		VkPhysicalDevice physical_device;
		VkDevice device;
		VkImageView swapchain_views[SWAP_CHAIN_IMAGE_COUNT];
		VkSwapchainKHR swapchain;
		uint32_t queue_family_index;
		VkQueue queues[QUEUE_COUNT];
		GLFWwindow* window;
	};
}