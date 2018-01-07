#include "resource_manager.h"

#include "utility.h"

#include "const_water_grid_size.h"
#include "const_pi.h" 

using namespace rcq;

template<>
void resource_manager::build<RES_TYPE_WATER>(base_resource* res, const char* build_info)
{
	auto w = reinterpret_cast<resource<RES_TYPE_WATER>*>(res->data);
	const auto build = reinterpret_cast<const resource<RES_TYPE_WATER>::build_info*>(build_info);

	w->grid_size_in_meters = build->grid_size_in_meters;

	uint32_t noise_size = resource<RES_TYPE_WATER>::GRID_SIZE*resource<RES_TYPE_WATER>::GRID_SIZE * sizeof(glm::vec4);

	uint64_t noise_staging_buffer_offset = m_mappable_memory.allocate(noise_size, 1);
	char* data = reinterpret_cast<char*>(m_mappable_memory.map(noise_staging_buffer_offset, noise_size));
	uint32_t __size;
	utility::read_file(build->filename, data, __size);
	m_mappable_memory.unmap();

	//create noise image
	{
		VkImageCreateInfo im = {};
		im.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		im.arrayLayers = 1;
		im.extent = { WATER_GRID_SIZE, WATER_GRID_SIZE, 1 };
		im.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		im.imageType = VK_IMAGE_TYPE_2D;
		im.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		im.mipLevels = 1;
		im.samples = VK_SAMPLE_COUNT_1_BIT;
		im.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		im.tiling = VK_IMAGE_TILING_OPTIMAL;
		im.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

		assert(vkCreateImage(m_base.device, &im, m_vk_alloc, &w->noise.image) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, w->noise.image, &mr);
		w->noise.offset = m_dl1_memory.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, w->noise.image, m_dl1_memory.handle(), w->noise.offset);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = w->noise.image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &w->noise.view) == VK_SUCCESS);
	}

	//create height and grad texture
	{
		VkImageCreateInfo im = {};
		im.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		im.arrayLayers = 2;
		im.extent = { resource<RES_TYPE_WATER>::GRID_SIZE, resource<RES_TYPE_WATER>::GRID_SIZE, 1 };
		im.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		im.imageType = VK_IMAGE_TYPE_2D;
		im.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		im.mipLevels = 1;
		im.samples = VK_SAMPLE_COUNT_1_BIT;
		im.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		im.tiling = VK_IMAGE_TILING_OPTIMAL;
		im.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		assert(vkCreateImage(m_base.device, &im, m_vk_alloc, &w->tex.image) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, w->tex.image, &mr);
		w->tex.offset = m_dl1_memory.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, w->tex.image, m_dl1_memory.handle(), 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = w->tex.image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 2;
		view.subresourceRange.levelCount = 1;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &w->tex.view) == VK_SUCCESS);
	}


	uint64_t params_staging_buffer_offset = m_mappable_memory.allocate(sizeof(resource<RES_TYPE_WATER>::fft_params_data), 1);

	resource<RES_TYPE_WATER>::fft_params_data* params = reinterpret_cast<resource<RES_TYPE_WATER>::fft_params_data*>
		(m_mappable_memory.map(params_staging_buffer_offset,
		sizeof(resource<RES_TYPE_WATER>::fft_params_data)));
	params->base_frequency = build->base_frequency;
	params->sqrtA = sqrtf(build->A);
	params->two_pi_per_L = glm::vec2(2.f*PI) / build->grid_size_in_meters;
	m_mappable_memory.unmap();

	//create fft params buffer
	{
		VkBufferCreateInfo b = {};
		b.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		b.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		b.size = sizeof(resource<RES_TYPE_WATER>::fft_params_data);
		b.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		assert(vkCreateBuffer(m_base.device, &b, m_vk_alloc, &w->fft_params_buffer) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, w->fft_params_buffer, &mr);
		w->fft_params_offset = m_dl0_memory.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, w->fft_params_buffer, m_dl0_memory.handle(), 0);
	}

	//transition layouts and copy from staging buffers
	{
		begin_build_cb();

		VkBufferCopy bc;
		bc.dstOffset = 0;
		bc.size = sizeof(resource<RES_TYPE_WATER>::fft_params_data);
		bc.srcOffset = params_staging_buffer_offset;

		vkCmdCopyBuffer(m_build_cb, m_staging_buffer, w->fft_params_buffer, 1, &bc);

		std::array<VkImageMemoryBarrier, 3> barriers = {};
		for (uint32_t i = 0; i < 3; ++i)
		{
			auto& b = barriers[i];
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.image = i >= 2 ? w->tex.image : w->noise.image;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = i >= 2 ? 2 : 1;
			b.subresourceRange.levelCount = 1;
		}

		barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barriers[0].srcAccessMask = 0;
		barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		barriers[2].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barriers[2].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[2].srcAccessMask = 0;
		barriers[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;


		vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, barriers.data());

		VkBufferImageCopy bic = {};
		bic.bufferImageHeight = 0;
		bic.bufferRowLength = 0;
		bic.bufferOffset = noise_staging_buffer_offset;
		bic.imageOffset = { 0, 0, 0 };
		bic.imageExtent = { resource<RES_TYPE_WATER>::GRID_SIZE, resource<RES_TYPE_WATER>::GRID_SIZE, 1 };
		bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bic.imageSubresource.baseArrayLayer = 0;
		bic.imageSubresource.layerCount = 1;
		bic.imageSubresource.mipLevel = 0;

		vkCmdCopyBufferToImage(m_build_cb, m_staging_buffer, w->noise.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);

		vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, barriers.data() + 1);

		vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, barriers.data() + 2);

		end_build_cb();
	}

	//create sampler
	{
		VkSamplerCreateInfo s = {};
		s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		s.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		s.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		s.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		s.anisotropyEnable = VK_TRUE;
		s.magFilter = VK_FILTER_LINEAR;
		s.minFilter = VK_FILTER_LINEAR;
		s.maxAnisotropy = 16;
		s.unnormalizedCoordinates = VK_FALSE;
		s.compareEnable = VK_FALSE;
		s.compareOp = VK_COMPARE_OP_ALWAYS;
		s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		s.mipLodBias = 0.f;
		s.minLod = 0.f;
		s.maxLod = 0.f;

		assert(vkCreateSampler(m_base.device, &s, m_vk_alloc, &w->sampler) == VK_SUCCESS);
	}

	//allocate dss
	{
		VkDescriptorSet dss[2];

		VkDescriptorSetLayout dsls[2] =
		{
			m_dsls[DSL_TYPE_WATER],
			m_dsls[DSL_TYPE_WATER_COMPUTE]
		};

		VkDescriptorSetAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pSetLayouts = dsls;
		alloc.descriptorSetCount = 2;
		alloc.descriptorPool = m_dp_pools[DSL_TYPE_WATER].use_dp(w->dp_index);

		assert(vkAllocateDescriptorSets(m_base.device, &alloc, dss) == VK_SUCCESS);

		w->ds = dss[0];
		w->fft_ds = dss[1];
	}

	//update dss
	{
		VkDescriptorBufferInfo fft_params;
		fft_params.buffer = w->fft_params_buffer;
		fft_params.offset = 0;
		fft_params.range = sizeof(resource<RES_TYPE_WATER>::fft_params_data);

		VkDescriptorImageInfo noise;
		noise.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		noise.imageView = w->noise.view;

		VkDescriptorImageInfo tex_comp;
		tex_comp.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		tex_comp.imageView = w->tex.view;

		VkDescriptorImageInfo tex_draw;
		tex_draw.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		tex_draw.imageView = w->tex.view;
		tex_draw.sampler = w->sampler;

		VkWriteDescriptorSet writes[4] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].dstArrayElement = 0;
		writes[0].dstBinding = 0;
		writes[0].dstSet = w->fft_ds;
		writes[0].pBufferInfo = &fft_params;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[1].dstArrayElement = 0;
		writes[1].dstBinding = 1;
		writes[1].dstSet = w->fft_ds;
		writes[1].pImageInfo = &noise;

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[2].dstArrayElement = 0;
		writes[2].dstBinding = 2;
		writes[2].dstSet = w->fft_ds;
		writes[2].pImageInfo = &tex_comp;

		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[3].dstArrayElement = 0;
		writes[3].dstBinding = 0;
		writes[3].dstSet = w->ds;
		writes[3].pImageInfo = &tex_draw;

		vkUpdateDescriptorSets(m_base.device, 4, writes, 0, nullptr);
	}

	wait_for_build_fence();

	m_mappable_memory.deallocate(noise_staging_buffer_offset);
	m_mappable_memory.deallocate(params_staging_buffer_offset);

	res->ready_bit.store(true, std::memory_order_release);
}