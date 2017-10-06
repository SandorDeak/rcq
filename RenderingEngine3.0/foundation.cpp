#include "foundation.h"
#include <fstream>


const VkAllocationCallbacks* rcq::host_memory_manager = nullptr;


const size_t rcq::DISASSEMBLER_COMMAND_QUEUE_MAX_SIZE = 1;
const size_t rcq::CORE_COMMAND_QUEUE_MAX_SIZE=1;
const size_t rcq::SHADOW_MAP_SIZE=128;



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

void rcq::create_staging_buffer(const base_info& base, VkDeviceSize size, VkBuffer & buffer, VkDeviceMemory & mem)
{
	VkBufferCreateInfo sb_info = {};
	sb_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sb_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sb_info.size = size;
	sb_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	if (vkCreateBuffer(base.device, &sb_info, host_memory_manager, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create staging buffer!");
	}

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(base.device, buffer, &memory_requirements);
	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type(base.physical_device, memory_requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(base.device, &alloc_info, host_memory_manager, &mem) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate staging buffer memory");
	}

	vkBindBufferMemory(base.device, buffer, mem, 0);
}

VkCommandBuffer rcq::begin_single_time_command(VkDevice device, VkCommandPool command_pool)
{
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandBufferCount = 1;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer cb;

	vkAllocateCommandBuffers(device, &alloc_info, &cb);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cb, &begin_info);
	return cb;
}

void rcq::end_single_time_command_buffer(VkDevice device, VkCommandPool cp, VkQueue queue_for_submit, VkCommandBuffer cb)
{
	vkEndCommandBuffer(cb);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cb;

	vkQueueSubmit(queue_for_submit, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue_for_submit);
	vkFreeCommandBuffers(device, cp, 1, &cb);
}