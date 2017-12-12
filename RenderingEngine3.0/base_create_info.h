#pragma once

#include "vulkan.h"

namespace rcq
{
	struct base_create_info
	{
		const char** validation_layers;
		uint32_t validation_layer_count;
		const char** instance_extensions;
		uint32_t instance_extensions_count;
		const char** device_extensions;
		uint32_t device_extensions_count;
		const char* window_name;
		bool enable_validation_layers;
		VkPhysicalDeviceFeatures device_features;
	};
}