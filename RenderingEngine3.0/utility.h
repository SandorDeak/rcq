#pragma once

#include <stdint.h>
#include "vulkan.h"
#include "vector.h"
#include "vertex.h"


namespace rcq::utility
{
	uint32_t find_memory_type(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties);

	void read_file(const char* filename, char* dst, uint32_t& size);

	void create_shaders(VkDevice device, const char** files, const VkShaderStageFlagBits* stages,
		VkPipelineShaderStageCreateInfo* shaders, uint32_t shader_count);

	void load_mesh(vector<vertex>& vertices, vector<uint32_t>& indices, vector<vertex_ext>& vertices_ext, bool calc_tb, 
		const char* filename, host_memory* memory);
}
