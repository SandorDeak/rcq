#include "resource_manager.h"

#include "stbimage.h"

using namespace rcq;

template<>
void resource_manager::build<RES_TYPE_MAT_OPAQUE>(base_resource* res, const char* build_info)
{
	const resource<RES_TYPE_MAT_OPAQUE>::build_info* build = reinterpret_cast<const resource<RES_TYPE_MAT_OPAQUE>::build_info*>(build_info);

	auto& mat = *reinterpret_cast<resource<RES_TYPE_MAT_OPAQUE>*>(res->data);


	begin_build_cb();

	//load textures
	uint64_t staging_buffer_offsets[TEX_TYPE_COUNT];
	uint32_t flag = 1;
	for (uint32_t i = 0; i<TEX_TYPE_COUNT; ++i)
	{
		if (flag & build->tex_flags)
		{
			auto& tex = mat.texs[i];

			int width, height, channels;
			stbi_uc* pixels = stbi_load(build->texfiles[i], &width, &height, &channels, STBI_rgb_alpha);

			assert(pixels);

			size_t im_size = width*height * 4;

			staging_buffer_offsets[i] = m_mappable_memory.allocate(im_size, 1);
			void* staging_buffer_data = reinterpret_cast<void*>(m_mappable_memory.map(staging_buffer_offsets[i], im_size));
			memcpy(staging_buffer_data, pixels, im_size);
			m_mappable_memory.unmap();
			stbi_image_free(pixels);

			uint32_t mip_level_count = 0;
			uint32_t mip_level_size = width < height ? height : width;
			while (mip_level_size != 0)
			{
				mip_level_size >>= 1;
				++mip_level_count;
			}

			//create image
			{
				VkImageCreateInfo im = {};
				im.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				im.arrayLayers = 1;
				im.extent.width = static_cast<uint32_t>(width);
				im.extent.height = static_cast<uint32_t>(height);
				im.extent.depth = 1;
				im.format = VK_FORMAT_R8G8B8A8_UNORM;
				im.imageType = VK_IMAGE_TYPE_2D;
				im.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				im.mipLevels = mip_level_count;
				im.samples = VK_SAMPLE_COUNT_1_BIT;
				im.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				im.tiling = VK_IMAGE_TILING_OPTIMAL;
				im.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

				assert(vkCreateImage(m_base.device, &im, m_vk_alloc, &tex.image) == VK_SUCCESS);
			}

			//allocate memory for image
			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, tex.image, &mr);

			tex.offset = m_dl1_memory.allocate(mr.size, mr.alignment);

			vkBindImageMemory(m_base.device, tex.image, m_dl1_memory.handle(), tex.offset);

			//transition level0 layout to transfer dst optimal
			{

				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.srcAccessMask = 0;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = tex.image;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.subresourceRange.levelCount = 1;

				vkCmdPipelineBarrier(
					m_build_cb,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);
			}
			//copy from staging buffer to level0
			{
				VkBufferImageCopy region = {};
				region.bufferImageHeight = 0;
				region.bufferRowLength = 0;
				region.bufferOffset = staging_buffer_offsets[i];
				region.imageExtent.depth = 1;
				region.imageExtent.width = width;
				region.imageExtent.height = height;
				region.imageOffset = { 0, 0, 0 };
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageSubresource.mipLevel = 0;

				vkCmdCopyBufferToImage(m_build_cb, m_staging_buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			}
			//fill mip levels
			for (uint32_t i = 0; i < mip_level_count - 1; ++i)
			{
				VkImageMemoryBarrier barriers[2] = {};
				for (uint32_t j = 0; j < 2; ++j)
				{
					barriers[j].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					barriers[j].image = tex.image;
					barriers[j].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barriers[j].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barriers[j].oldLayout = j == 0 ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
					barriers[j].newLayout = j == 0 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					barriers[j].srcAccessMask = j == 0 ? VK_ACCESS_TRANSFER_WRITE_BIT : 0;
					barriers[j].dstAccessMask = j == 0 ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
					barriers[j].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					barriers[j].subresourceRange.baseArrayLayer = 0;
					barriers[j].subresourceRange.layerCount = 1;
					barriers[j].subresourceRange.baseMipLevel = i + j;
					barriers[j].subresourceRange.levelCount = 1;
				}
				vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr, 0, nullptr, 2, barriers);

				VkImageBlit blit = {};
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { width, height, 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
				blit.srcSubresource.mipLevel = i;

				width >>= 1;
				height >>= 1;

				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { width, height, 1 };
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;
				blit.dstSubresource.mipLevel = i + 1;

				vkCmdBlitImage(m_build_cb, tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &blit, VK_FILTER_LINEAR);
			}

			//transition all level layout to shader read only optimal
			VkImageMemoryBarrier barriers[2] = {};
			for (uint32_t i = 0; i < 2; ++i)
			{
				barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barriers[i].srcAccessMask = i == 0 ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
				barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barriers[i].image = tex.image;
				barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barriers[i].oldLayout = i == 0 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barriers[i].subresourceRange.baseArrayLayer = 0;
				barriers[i].subresourceRange.baseMipLevel = i == 0 ? 0 : mip_level_count - 1;
				barriers[i].subresourceRange.layerCount = 1;
				barriers[i].subresourceRange.levelCount = i == 0 ? mip_level_count - 1 : 1;
			}

			vkCmdPipelineBarrier(
				m_build_cb,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				2, barriers
			);

			//create image view
			{
				VkImageViewCreateInfo view = {};
				view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				view.format = VK_FORMAT_R8G8B8A8_UNORM;
				view.image = tex.image;
				view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				view.subresourceRange.baseArrayLayer = 0;
				view.subresourceRange.baseMipLevel = 0;
				view.subresourceRange.layerCount = 1;
				view.subresourceRange.levelCount = mip_level_count;
				view.viewType = VK_IMAGE_VIEW_TYPE_2D;

				assert(vkCreateImageView(m_base.device, &view, m_vk_alloc,
					&tex.view) == VK_SUCCESS);
			}

			//create sampler
			{
				VkSamplerCreateInfo s = {};
				s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				s.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				s.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				s.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				s.anisotropyEnable = VK_TRUE;
				s.compareEnable = VK_FALSE;
				s.maxAnisotropy = 16.f;
				s.magFilter = VK_FILTER_LINEAR;
				s.minFilter = VK_FILTER_LINEAR;
				s.minLod = 0.f;
				s.maxLod = static_cast<float>(mip_level_count - 1);
				s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				s.unnormalizedCoordinates = VK_FALSE;

				assert(vkCreateSampler(m_base.device, &s, m_vk_alloc, &tex.sampler) == VK_SUCCESS);
			}
		}
		else
		{
			//staging_buffer_offsets[i] = 0;
			mat.texs[i].image = VK_NULL_HANDLE;
		}
		flag = flag << 1;
	}

	//create material data buffer
	{
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer.size = sizeof(resource<RES_TYPE_MAT_OPAQUE>::data);
		buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		assert(vkCreateBuffer(m_base.device, &buffer, m_vk_alloc, &mat.data_buffer) == VK_SUCCESS);
	}

	//allocate memory for material data buffer
	{
		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, mat.data_buffer, &mr);
		mat.data_offset = m_dl0_memory.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, mat.data_buffer, m_dl0_memory.handle(), mat.data_offset);
		/*VkMemoryAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc.allocationSize = mr.size;
		alloc.memoryTypeIndex = MEMORY_TYPE_DL0;
		VkDeviceMemory m;
		assert(vkAllocateMemory(m_base.device, &alloc, m_vk_alloc, &m) == VK_SUCCESS);
		vkBindBufferMemory(m_base.device, mat.data_buffer, m, 0);*/
	}

	//copy to staging buffer
	VkDeviceSize staging_buffer_offset = m_mappable_memory.allocate(sizeof(resource<RES_TYPE_MAT_OPAQUE>::data),
		alignof(resource<RES_TYPE_MAT_OPAQUE>::data));
	auto p_data = reinterpret_cast<resource<RES_TYPE_MAT_OPAQUE>::data*>(
		m_mappable_memory.map(staging_buffer_offset, sizeof(resource<RES_TYPE_MAT_OPAQUE>::data)));

	/*VkBufferCreateInfo sb = {};
	sb.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sb.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sb.size = sizeof(resource<RES_TYPE_MAT_OPAQUE>::data);
	sb.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VkBuffer staging_b;
	assert(vkCreateBuffer(m_base.device, &sb, m_vk_alloc, &staging_b) == VK_SUCCESS);
	VkMemoryRequirements mr;
	vkGetBufferMemoryRequirements(m_base.device, staging_b, &mr);
	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = mr.size;
	alloc.memoryTypeIndex = MEMORY_TYPE_HVC;
	VkDeviceMemory m;
	assert(vkAllocateMemory(m_base.device, &alloc, m_vk_alloc, &m) == VK_SUCCESS);
	vkBindBufferMemory(m_base.device, staging_b, m, 0);
	void* raw_data;
	vkMapMemory(m_base.device, m, 0, sizeof(resource<RES_TYPE_MAT_OPAQUE>::data), 0, &raw_data);
	auto data = reinterpret_cast<resource<RES_TYPE_MAT_OPAQUE>::data*>(raw_data);*/

	resource<RES_TYPE_MAT_OPAQUE>::data data;
	data.color = build->color;
	data.flags = build->tex_flags;
	data.height_scale = build->height_scale;
	data.metal = build->metal;
	data.roughness = build->roughness;
	memcpy(p_data, &data, sizeof(data));
	m_mappable_memory.unmap();

	//copy to material data buffer
	{
		VkBufferCopy region = {};
		region.dstOffset = 0;
		region.size = sizeof(resource<RES_TYPE_MAT_OPAQUE>::data);
		region.srcOffset = staging_buffer_offset;

		vkCmdCopyBuffer(m_build_cb, m_staging_buffer, mat.data_buffer, 1, &region);
	}

	end_build_cb();

	//allocate descriptor set
	{
		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &m_dsls[DSL_TYPE_MAT_OPAQUE];
 		alloc_info.descriptorPool = m_dp_pools[DSL_TYPE_MAT_OPAQUE].use_dp(mat.dp_index); 

		assert(vkAllocateDescriptorSets(m_base.device, &alloc_info, &mat.ds) == VK_SUCCESS);
	}

	//update descriptor set
	{
		VkDescriptorBufferInfo buffer_info = {};
		buffer_info.buffer = mat.data_buffer;
		buffer_info.offset = 0;
		buffer_info.range = sizeof(resource<RES_TYPE_MAT_OPAQUE>::data);

		VkDescriptorImageInfo tex_info[TEX_TYPE_COUNT];
		VkWriteDescriptorSet write[TEX_TYPE_COUNT + 1] = {};

		write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[0].descriptorCount = 1;
		write[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write[0].dstArrayElement = 0;
		write[0].dstBinding = 0;
		write[0].dstSet = mat.ds;
		write[0].pBufferInfo = &buffer_info;

		uint32_t write_index = 1;
		uint32_t tex_info_index = 0;
		flag = 1;
		for (uint32_t i = 0; i < TEX_TYPE_COUNT; ++i)
		{
			if (flag & build->tex_flags)
			{
				tex_info[tex_info_index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				tex_info[tex_info_index].imageView = mat.texs[i].view;
				tex_info[tex_info_index].sampler = mat.texs[i].sampler;

				write[write_index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write[write_index].descriptorCount = 1;
				write[write_index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				write[write_index].dstArrayElement = 0;
				write[write_index].dstBinding = i + 1;
				write[write_index].dstSet = mat.ds;
				write[write_index].pImageInfo = &tex_info[tex_info_index];

				++tex_info_index;
				++write_index;
			}
			flag = flag << 1;
		}

		vkUpdateDescriptorSets(m_base.device, write_index, write, 0, nullptr);
	}

	wait_for_build_fence();

	res->ready_bit.store(true, std::memory_order_release);

	/*m_mappable_memory.deallocate(staging_buffer_offset);
	for (uint32_t i = 0; i < TEX_TYPE_COUNT; ++i)
	{
		if (staging_buffer_offsets[i] != 0)
			m_mappable_memory.deallocate(staging_buffer_offsets[i]);
	}*/
}