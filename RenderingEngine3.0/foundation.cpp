#include "foundation.h"
#include <fstream>


const VkAllocationCallbacks* rcq::host_memory_manager = nullptr;

const size_t rcq::POOL_MAT_STATIC_SIZE = 64; //deprecated
const size_t rcq::POOL_MAT_DYNAMIC_SIZE = 64; //deprecated
const size_t rcq::POOL_TR_STATIC_SIZE = 64; //deprecated
const size_t rcq::POOL_TR_DYNAMIC_SIZE = 64; //deprecated

const size_t rcq::DISASSEMBLER_COMMAND_QUEUE_MAX_SIZE = 4;

std::vector<char> rcq::read_file(const std::string& filename)
{
	std::ifstream file(filename.c_str(), std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("failed to open file!");

	size_t size = (size_t)file.tellg();
	std::vector<char> data(size);
	file.seekg(0);
	file.read(data.data(), size);
	file.close();

	return data;
}

VkShaderModule rcq::create_shader_module(VkDevice device, const std::vector<char>& code)
{
	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = code.size();
	info.pCode = reinterpret_cast<const uint32_t*>(code.data());
	
	VkShaderModule shader_m;
	if (vkCreateShaderModule(device, &info, rcq::host_memory_manager, &shader_m) != VK_SUCCESS)
		throw std::runtime_error("failed to create shader module!");
	return shader_m;
}

VkFormat rcq::find_support_format(VkPhysicalDevice device, const std::vector<VkFormat>& candidates, 
	VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (auto format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(device, format, &props);
		if (tiling == VK_IMAGE_TILING_OPTIMAL && ((props.optimalTilingFeatures & features) == features))
			return format;
		else if (tiling == VK_IMAGE_TILING_LINEAR && ((props.linearTilingFeatures & features) == features))
			return format;
	}

	throw std::runtime_error("failed to found supported format!");
}

uint32_t rcq::find_memory_type(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
	{
		if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties))
			return i;
	}

	throw std::runtime_error("failed to find suitable memory type!");
}
