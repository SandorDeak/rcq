#include "engine.h"

#include "const_dir_shadow_map_size.h"
#include "const_environment_map_size.h"
#include "const_swap_chain_image_extent.h"
#include "const_bloom_image_size_factor.h"

namespace rcq
{
	void engine::create_buffers_and_images()
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(m_base.physical_device, &props);
		size_t ub_alignment = static_cast<size_t>(props.limits.minUniformBufferOffsetAlignment);
		m_res_data.calc_offset_and_size(ub_alignment);

		size_t size = m_res_data.size;

		//res data staging buffer
		{
			VkBufferCreateInfo buffer = {};
			buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			buffer.size = size;
			buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

			assert(vkCreateBuffer(m_base.device, &buffer, m_vk_alloc, &m_res_data.staging_buffer) == VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetBufferMemoryRequirements(m_base.device, m_res_data.staging_buffer, &mr);
			m_res_data.staging_buffer_offset = m_mappable_memory.allocate(mr.size, mr.alignment);
			vkBindBufferMemory(m_base.device, m_res_data.staging_buffer, m_mappable_memory.handle(),
				m_res_data.staging_buffer_offset);
			m_res_data.set_pointers(m_mappable_memory.map(m_res_data.staging_buffer_offset, size));
		}

		//res data buffer
		{
			VkBufferCreateInfo buffer = {};
			buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			buffer.size = size;
			buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

			assert(vkCreateBuffer(m_base.device, &buffer, m_vk_alloc, &m_res_data.buffer) == VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetBufferMemoryRequirements(m_base.device, m_res_data.buffer, &mr);
			m_res_data.buffer_offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindBufferMemory(m_base.device, m_res_data.buffer, m_device_memory.handle(), m_res_data.buffer_offset);
		}

		//environment map depthstencil
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			image.arrayLayers = 6;
			image.extent.depth = 1;
			image.extent.height = ENVIRONMENT_MAP_SIZE;
			image.extent.width = ENVIRONMENT_MAP_SIZE;
			image.format = VK_FORMAT_D32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image)
				== VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_D32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 6;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].view)
				== VK_SUCCESS);
		}

		//environment map
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			image.arrayLayers = 6;
			image.extent.depth = 1;
			image.extent.height = ENVIRONMENT_MAP_SIZE;
			image.extent.width = ENVIRONMENT_MAP_SIZE;
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image)
				== VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 6;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP].view)
				== VK_SUCCESS);
		}

		//gbuffer pos roughness
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.extent.depth = 1;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image)
				== VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view)
				== VK_SUCCESS);
		}

		//gbuffer F0 ssao
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.extent.depth = 1;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image)
				== VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].view)
				== VK_SUCCESS);
		}

		//gbuffer albedo ssds
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.extent.depth = 1;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image)
				== VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_GB_METALNESS_SSDS].view)
				== VK_SUCCESS);
		}

		//gbuffer normal ao
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.extent.depth = 1;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_GB_NORMAL_AO].image)
				== VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_NORMAL_AO].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_NORMAL_AO].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_GB_NORMAL_AO].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_GB_NORMAL_AO].view)
				== VK_SUCCESS);
		}

		//preimage
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.extent.depth = 1;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_PREIMAGE].image)
				== VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_PREIMAGE].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_PREIMAGE].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_PREV_IMAGE].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_PREV_IMAGE].view) == VK_SUCCESS);
		}

		//gbuffer depth
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.extent.depth = 1;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_GB_DEPTH].image)
				== VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_DEPTH].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_DEPTH].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			view.image = m_res_image[RES_IMAGE_GB_DEPTH].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_GB_DEPTH].view)
				== VK_SUCCESS);
		}

		//dir shadow map
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = FRUSTUM_SPLIT_COUNT;
			image.extent.depth = 1;
			image.extent.height = DIR_SHADOW_MAP_SIZE;
			image.extent.width = DIR_SHADOW_MAP_SIZE;
			image.format = VK_FORMAT_D32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image)
				== VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_D32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = FRUSTUM_SPLIT_COUNT;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_DIR_SHADOW_MAP].view)
				== VK_SUCCESS);
		}

		//prev_image
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.depth = 1;
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_PREV_IMAGE].image) == VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_PREV_IMAGE].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_PREV_IMAGE].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_PREV_IMAGE].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_PREV_IMAGE].view) == VK_SUCCESS);
		}

		//refraction image
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.depth = 1;
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_REFRACTION_IMAGE].image) == VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_REFRACTION_IMAGE].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_REFRACTION_IMAGE].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_REFRACTION_IMAGE].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_REFRACTION_IMAGE].view) == VK_SUCCESS);
		}

		//ssr ray casting coords
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.arrayLayers = 1;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.depth = 1;
			image.format = VK_FORMAT_R32_SINT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].image) == VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32_SINT;
			view.image = m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].view) == VK_SUCCESS);
		}

		//ss dir shadow map
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.extent.depth = 1;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.format = VK_FORMAT_D32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image)
				== VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_D32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].view)
				== VK_SUCCESS);
		}

		//ssao map
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.arrayLayers = 1;
			image.extent.depth = 1;
			image.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height;
			image.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width;
			image.format = VK_FORMAT_D32_SFLOAT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_SSAO_MAP].image)
				!= VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_SSAO_MAP].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_SSAO_MAP].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_D32_SFLOAT;
			view.image = m_res_image[RES_IMAGE_SSAO_MAP].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_SSAO_MAP].view)
				== VK_SUCCESS);
		}

		//bloom blur
		{
			VkImageCreateInfo im = {};
			im.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			im.arrayLayers = 2;
			im.extent.width = SWAP_CHAIN_IMAGE_EXTENT.width / BLOOM_IMAGE_SIZE_FACTOR;
			im.extent.height = SWAP_CHAIN_IMAGE_EXTENT.height / BLOOM_IMAGE_SIZE_FACTOR;
			im.extent.depth = 1;
			im.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			im.imageType = VK_IMAGE_TYPE_2D;
			im.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			im.mipLevels = 1;
			im.samples = VK_SAMPLE_COUNT_1_BIT;
			im.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			im.tiling = VK_IMAGE_TILING_OPTIMAL;
			im.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			assert(vkCreateImage(m_base.device, &im, m_vk_alloc, &m_res_image[RES_IMAGE_BLOOM_BLUR].image) == VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_BLOOM_BLUR].image, &mr);
			uint64_t offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_BLOOM_BLUR].image, m_device_memory.handle(),
				offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 2;
			view.subresourceRange.levelCount = 1;

			view.image = m_res_image[RES_IMAGE_BLOOM_BLUR].image;
			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_BLOOM_BLUR].view) == VK_SUCCESS);
		}
	}
}