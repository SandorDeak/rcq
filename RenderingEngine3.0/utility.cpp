#include "utility.h"

#include <assert.h>
#include <fstream>
#include "vector.h"
#include "os_memory.h"

uint32_t rcq::utility::find_memory_type(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties)
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

void rcq::utility::read_file(const char* filename, char* dst, uint32_t& size)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	assert(file.is_open());

	size = (uint32_t)file.tellg();

	file.seekg(0);
	file.read(dst, size);
	file.close();
}

void rcq::utility::create_shaders(VkDevice device, const char** files, const VkShaderStageFlagBits* stages,
	VkPipelineShaderStageCreateInfo* shaders, uint32_t shader_count)
{
	for (uint32_t i = 0; i < shader_count; ++i)
	{
		std::ifstream file(files[i], std::ios::ate | std::ios::binary);
		assert(file.is_open());

		size_t size = static_cast<size_t>(file.tellg());
		vector<char> code(&OS_MEMORY, size);

		file.seekg(0);
		file.read(code.data(), size);
		file.close();
		VkShaderModuleCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = size; 
		info.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shader_module;
		assert(vkCreateShaderModule(device, &info, nullptr, &shader_module) == VK_SUCCESS);

		shaders[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaders[i].module = shader_module;
		shaders[i].pName = "main";
		shaders[i].stage = stages[i];
	}
}