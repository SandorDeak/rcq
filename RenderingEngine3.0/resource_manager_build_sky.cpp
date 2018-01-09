#include "resource_manager.h"

#include "utility.h"

using namespace rcq;

template<>
void resource_manager::build<RES_TYPE_SKY>(base_resource* res, const char* build_info)
{
	resource<RES_TYPE_SKY>* s = reinterpret_cast<resource<RES_TYPE_SKY>*>(res->data);
	const auto build = reinterpret_cast<const resource<RES_TYPE_SKY>::build_info*>(build_info);

	const char* suffix[3] = { "_Rayleigh.sky", "_Mie.sky", "_transmittance.sky" };

	uint64_t sky_im_size = build->sky_image_size.x*build->sky_image_size.y*build->sky_image_size.z * sizeof(glm::vec4);
	uint64_t transmittance_im_size = build->transmittance_image_size.x*build->transmittance_image_size.y * sizeof(glm::vec4);

	uint64_t staging_buffer_offsets[3];

	begin_build_cb();

	for (uint32_t i = 0; i < 3; ++i)
	{

		//load image from file to staging buffer;
		char filename[256];
		strcpy_s(filename, build->filename);
		strcat_s(filename, suffix[i]);

		uint64_t size = i == 2 ? transmittance_im_size : sky_im_size;

		staging_buffer_offsets[i] = m_mappable_memory.allocate(size, 1);
		char* data = reinterpret_cast<char*>(m_mappable_memory.map(staging_buffer_offsets[i], size));
		uint32_t __size;
		utility::read_file(filename, data, __size);
		m_mappable_memory.unmap();

		VkExtent3D extent;
		extent.width = i == 2 ? build->transmittance_image_size.x : build->sky_image_size.x;
		extent.height = i == 2 ? build->transmittance_image_size.y : build->sky_image_size.y;
		extent.depth = i == 2 ? 1 : build->sky_image_size.z;

		//create texture
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.extent = extent;
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.arrayLayers = 1;
			image.imageType = i == 2 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &s->tex[i].image) == VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, s->tex[i].image, &mr);
			s->tex[i].offset = m_dl1_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, s->tex[i].image, m_dl1_memory.handle(), s->tex[i].offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = s->tex[i].image;
			view.viewType = i == 2 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &s->tex[i].view) == VK_SUCCESS);

			//s->tex[i].sampler = m_samplers[SAMPLER_TYPE_CLAMP];
		}

		//create sampler
		{
			VkSamplerCreateInfo sampler = {};
			sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler.anisotropyEnable = VK_FALSE;
			sampler.maxAnisotropy = 16.f;
			sampler.compareEnable = VK_FALSE;
			sampler.magFilter = VK_FILTER_LINEAR;
			sampler.maxLod = 0.f;
			sampler.minFilter = VK_FILTER_LINEAR;
			sampler.minLod = 0.f;
			sampler.mipLodBias = 0.f;
			sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

			assert(vkCreateSampler(m_base.device, &sampler, m_vk_alloc, &s->sampler) == VK_SUCCESS);
		}

		//transition to transfer dst optimal
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = s->tex[i].image;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = 0;
			b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
				0, nullptr, 1, &b);
		}

		//copy from staging buffer
		{
			VkBufferImageCopy region = {};
			region.bufferImageHeight = 0;
			region.bufferOffset = staging_buffer_offsets[i];
			region.bufferRowLength = 0;
			region.imageExtent = extent;
			region.imageOffset = { 0,0,0 };
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.mipLevel = 0;

			vkCmdCopyBufferToImage(m_build_cb, m_staging_buffer, s->tex[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		//transition to shader read only optimal
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = s->tex[i].image;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
				0, nullptr, 1, &b);
		}

	}

	end_build_cb();

	//allocate descriptor set
	{
		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &m_dsls[DSL_TYPE_SKY];
		alloc_info.descriptorPool = m_dp_pools[DSL_TYPE_SKY].use_dp(s->dp_index);

		assert(vkAllocateDescriptorSets(m_base.device, &alloc_info, &s->ds) == VK_SUCCESS);
	}

	//update ds
	{
		VkDescriptorImageInfo Rayleigh_tex;
		Rayleigh_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Rayleigh_tex.imageView = s->tex[0].view;
		Rayleigh_tex.sampler = s->sampler;

		VkDescriptorImageInfo Mie_tex;
		Mie_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Mie_tex.imageView = s->tex[1].view;
		Mie_tex.sampler = s->sampler;

		VkDescriptorImageInfo transmittance_tex;
		transmittance_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		transmittance_tex.imageView = s->tex[2].view;
		transmittance_tex.sampler = s->sampler;

		std::array<VkWriteDescriptorSet, 3> write = {};
		write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[0].descriptorCount = 1;
		write[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[0].dstArrayElement = 0;
		write[0].dstBinding = 0;
		write[0].dstSet = s->ds;
		write[0].pImageInfo = &Rayleigh_tex;

		write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[1].descriptorCount = 1;
		write[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[1].dstArrayElement = 0;
		write[1].dstBinding = 1;
		write[1].dstSet = s->ds;
		write[1].pImageInfo = &Rayleigh_tex;

		write[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[2].descriptorCount = 1;
		write[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[2].dstArrayElement = 0;
		write[2].dstBinding = 2;
		write[2].dstSet = s->ds;
		write[2].pImageInfo = &transmittance_tex;

		vkUpdateDescriptorSets(m_base.device, write.size(), write.data(), 0, nullptr);
	}

	wait_for_build_fence();
	res->ready_bit.store(true, std::memory_order_release);

	/*for (uint32_t i = 0; i < 3; ++i)
		m_mappable_memory.deallocate(staging_buffer_offsets[i]);*/
}