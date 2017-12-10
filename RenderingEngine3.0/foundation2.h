#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_SWIZZLE
#include<glm\glm.hpp>
#include <glm\gtx\hash.hpp>
#include <glm\gtx\transform.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>

//#include "slot_map.h"

#include <atomic>

#include "list.h"
#include "stack.h"
#include "array.h"
#include "host_memory.h"


namespace rcq
{	

	struct render_settings
	{
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec3 pos;
		float near;
		float far;

		glm::vec3 light_dir;
		glm::vec3 irradiance;
		glm::vec3 ambient_irradiance;

		glm::vec2 wind;
		float time;
	};

	

	

	//should be a functor maybe
	uint32_t find_memory_type(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memory_properties;
		vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);

		for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
		{
			if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties))
				return i;
		}

		assert(false);
	}

} //namespace rcq