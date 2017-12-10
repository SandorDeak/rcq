#pragma once

#include <stdint.h>
#include <vulkan\vulkan.h>


namespace rcq
{
	/*struct consts
	{
		static const uint32_t water_grid_size = 1024;
		static const uint32_t bloom_image_size_factor = 4;
		static const uint32_t swap_chain_size = 2;
		static const uint32_t max_alignment = 256;
		static const uint32_t environment_map_size = 128;
		static const uint32_t dir_shadow_map_size = 1024;
		static const uint32_t frustum_split_count = 4;
	};*/

	namespace utility
	{
		uint32_t find_memory_type(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties);

		void read_file(const char* filename, char* dst, uint32_t& size);

		void create_shaders(VkDevice device, const char** files, const VkShaderStageFlagBits* stages,
			VkPipelineShaderStageCreateInfo* shaders, uint32_t shader_count);
	}
}