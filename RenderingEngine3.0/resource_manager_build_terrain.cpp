#include "resource_manager.h"

using namespace rcq;

template<>
void resource_manager::build<RES_TYPE_TERRAIN>(base_resource* res, const char* build_info)
{
	auto t = reinterpret_cast<resource<RES_TYPE_TERRAIN>*>(res->data);
	auto build = reinterpret_cast<const resource<RES_TYPE_TERRAIN>::build_info*>(build_info);

	new(&t->tex.files) vector<std::ifstream>(&m_host_memory, build->mip_level_count);

	t->level0_tile_size = build->level0_tile_size;

	//open files
	for (uint32_t i = 0; i < build->mip_level_count; ++i)
	{
		char filename[128];
		char num[2];
		strcpy(filename, build->filename);
		itoa(i, num, 10);
		strcat(filename, num);
		strcat(filename, ".terr");
		new(&t->tex.files[i]) std::ifstream(filename, std::ios::binary);
		assert(t->tex.files[i].is_open());
	}

	//create image
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.width = build->level0_image_size.x;
		image.extent.height = build->level0_image_size.y;
		image.extent.depth = 1;
		image.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
		image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = build->mip_level_count;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
		image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &t->tex.image) == VK_SUCCESS);
	}

	VkSemaphore binding_finished_s;
	//allocate and bind mip tail and dummy page memory
	{
		uint32_t mr_count;
		vkGetImageSparseMemoryRequirements(m_base.device, t->tex.image, &mr_count, nullptr);
		assert(mr_count == 1);
		VkSparseImageMemoryRequirements sparse_mr;
		vkGetImageSparseMemoryRequirements(m_base.device, t->tex.image, &mr_count, &sparse_mr);

		assert(sparse_mr.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_NONSTANDARD_BLOCK_SIZE_BIT == 0);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, t->tex.image, &mr);

		uint64_t dummy_page_size = sparse_mr.formatProperties.imageGranularity.width*sparse_mr.formatProperties.imageGranularity.height;
		t->tex.mip_tail_offset = m_device_memory.allocate(sparse_mr.imageMipTailSize, mr.alignment);
		t->tex.dummy_page_offset = m_device_memory.allocate(dummy_page_size, mr.alignment);

		//mip tail bind
		VkSparseMemoryBind mip_tail = {};
		mip_tail.memory = m_device_memory.handle();
		mip_tail.memoryOffset = t->tex.mip_tail_offset;
		mip_tail.resourceOffset = sparse_mr.imageMipTailOffset;
		mip_tail.size = sparse_mr.imageMipTailSize;

		VkSparseImageOpaqueMemoryBindInfo mip_tail_bind = {};
		mip_tail_bind.bindCount = 1;
		mip_tail_bind.image = t->tex.image;
		mip_tail_bind.pBinds = &mip_tail;


		glm::vec2 tile_count = build->level0_image_size / build->level0_tile_size;
		glm::uvec2 page_size(sparse_mr.formatProperties.imageGranularity.width,
			sparse_mr.formatProperties.imageGranularity.height);
		glm::vec2 page_count_in_level0_image = build->level0_image_size / page_size;

		uint64_t page_count = page_count_in_level0_image.x*page_count_in_level0_image.y;

		page_count *= static_cast<uint64_t>((1.f - powf(0.25f, static_cast<float>(build->mip_level_count))) / 0.75f);


		vector<VkSparseImageMemoryBind> page_binds(&m_host_memory, page_count);

		uint64_t page_index = 0;
		glm::uvec2 tile_size_in_pages = build->level0_tile_size / page_size;
		for (uint32_t mip_level = 0; mip_level < build->mip_level_count; ++mip_level)
		{
			for (uint32_t i = 0; i < tile_count.x; ++i)
			{
				for (uint32_t j = 0; j < tile_count.y; ++j)
				{
					//at tile(i,j) in mip level mip_level
					VkOffset3D tile_offset =
					{
						static_cast<int32_t>(i*tile_size_in_pages.x*page_size.x),
						static_cast<int32_t>(j*tile_size_in_pages.y*page_size.y),
						0
					};

					for (uint32_t u = 0; u < tile_size_in_pages.x; ++u)
					{
						for (uint32_t v = 0; v < tile_size_in_pages.y; ++v)
						{
							//at page (u,v)
							VkOffset3D page_offset_in_tile =
							{
								static_cast<int32_t>(u*page_size.x),
								static_cast<int32_t>(v*page_size.y),
								0
							};
							page_binds[page_index].memory = m_device_memory.handle();
							page_binds[page_index].memoryOffset = t->tex.dummy_page_offset;
							page_binds[page_index].offset =
							{
								tile_offset.x + page_offset_in_tile.x,
								tile_offset.y + page_offset_in_tile.y,
								0
							};
							page_binds[page_index].extent =
							{
								page_size.x,
								page_size.y,
								1
							};
							page_binds[page_index].subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							page_binds[page_index].subresource.arrayLayer = 0;
							page_binds[page_index].subresource.mipLevel = mip_level;

							++page_index;
						}
					}
				}
			}
			tile_size_in_pages /= 2;
		}

		VkSparseImageMemoryBindInfo image_bind;
		image_bind.bindCount = page_binds.size();
		image_bind.pBinds = page_binds.data();
		image_bind.image = t->tex.image;

		VkBindSparseInfo bind_info = {};
		bind_info.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
		bind_info.imageOpaqueBindCount = 1;
		bind_info.pImageOpaqueBinds = &mip_tail_bind;
		bind_info.imageBindCount = 1;
		bind_info.pImageBinds = &image_bind;

		VkSemaphoreCreateInfo s = {};
		s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		assert(vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &binding_finished_s) == VK_SUCCESS);

		bind_info.signalSemaphoreCount = 1;
		bind_info.pSignalSemaphores = &binding_finished_s;

		vkQueueBindSparse(m_base.transfer_queue, 1, &bind_info, VK_NULL_HANDLE);
	}

	uint64_t data_staging_buffer_offset;
	//create terrain data buffer
	{
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer.size = sizeof(resource<RES_TYPE_TERRAIN>::data);
		buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		assert(vkCreateBuffer(m_base.device, &buffer, m_vk_alloc, &t->data_buffer) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, t->data_buffer, &mr);
		t->data_offset = m_device_memory.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, t->data_buffer, m_device_memory.handle(), t->data_offset);

		data_staging_buffer_offset = m_mappable_memory.allocate(sizeof(resource<RES_TYPE_TERRAIN>::data),
			alignof(resource<RES_TYPE_TERRAIN>::data));

		auto data = reinterpret_cast<resource<RES_TYPE_TERRAIN>::data*>(m_mappable_memory.map(data_staging_buffer_offset,
			sizeof(resource<RES_TYPE_TERRAIN>::data)));

		data->height_scale = build->size_in_meters.y;
		data->mip_level_count = static_cast<float>(build->mip_level_count);
		data->terrain_size_in_meters = { build->size_in_meters.x, build->size_in_meters.z };
		data->tile_count = static_cast<glm::ivec2>(build->level0_image_size / build->level0_tile_size);
		data->meter_per_tile_size_length = glm::vec2(build->size_in_meters.x, build->size_in_meters.z) /
			static_cast<glm::vec2>(build->level0_image_size / build->level0_tile_size);

		m_mappable_memory.unmap();
	}

	uint64_t request_data_staging_buffer_offset;
	//create terrain request buffer
	{
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer.size = sizeof(resource<RES_TYPE_TERRAIN>::request_data);
		buffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		assert(vkCreateBuffer(m_base.device, &buffer, m_vk_alloc, &t->request_data_buffer) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, t->request_data_buffer, &mr);
		t->request_data_offset = m_device_memory.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, t->request_data_buffer, m_device_memory.handle(), t->request_data_offset);

		request_data_staging_buffer_offset = m_mappable_memory.allocate(sizeof(resource<RES_TYPE_TERRAIN>::request_data),
			alignof(resource<RES_TYPE_TERRAIN>::request_data));
		auto data = reinterpret_cast<resource<RES_TYPE_TERRAIN>::request_data*>(m_mappable_memory.map(request_data_staging_buffer_offset,
			sizeof(resource<RES_TYPE_TERRAIN>::request_data)));

		data->mip_level_count = static_cast<float>(build->mip_level_count);
		//data->request_count = 0;
		data->tile_size_in_meter = glm::vec2(build->size_in_meters.x, build->size_in_meters.z) /
			static_cast<glm::vec2>(build->level0_image_size / build->level0_tile_size);

		m_mappable_memory.unmap();
	}


	//record cb
	{
		begin_build_cb();

		//transition to general
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = t->tex.image;
			b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			b.srcAccessMask = 0;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = build->mip_level_count;

			vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//copy from data staging buffer
		{
			VkBufferCopy region;
			region.dstOffset = 0;
			region.size = sizeof(resource<RES_TYPE_TERRAIN>::data);
			region.srcOffset = data_staging_buffer_offset;

			vkCmdCopyBuffer(m_build_cb, m_staging_buffer, t->data_buffer, 1, &region);
		}

		//copy from request data staging buffer
		{
			VkBufferCopy region;
			region.dstOffset = 0;
			region.size = sizeof(resource<RES_TYPE_TERRAIN>::request_data);
			region.srcOffset = request_data_staging_buffer_offset;

			vkCmdCopyBuffer(m_build_cb, m_staging_buffer, t->request_data_buffer, 1, &region);
		}

		VkPipelineStageFlags wait_flag = VK_PIPELINE_STAGE_TRANSFER_BIT;
		end_build_cb(&binding_finished_s, &wait_flag, 1);

		wait_for_build_fence();

		vkDestroySemaphore(m_base.device, binding_finished_s, m_vk_alloc);

		m_mappable_memory.deallocate(data_staging_buffer_offset);
		m_mappable_memory.deallocate(request_data_staging_buffer_offset);
	}

	//create image view
	{
		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = t->tex.image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.levelCount = build->mip_level_count;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &t->tex.view) == VK_SUCCESS);
	}

	//create sampler
	{
		VkSamplerCreateInfo s = {};
		s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		s.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.anisotropyEnable = VK_FALSE;
		s.compareEnable = VK_FALSE;
		s.magFilter = VK_FILTER_NEAREST;
		s.minFilter = VK_FILTER_NEAREST;
		s.maxLod = static_cast<float>(build->mip_level_count - 1u);
		s.minLod = 0.f;
		s.mipLodBias = 0.f;
		s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		s.unnormalizedCoordinates = VK_FALSE;

		assert(vkCreateSampler(m_base.device, &s, m_vk_alloc, &t->tex.sampler) == VK_SUCCESS);
	}

	//allocate dss
	{
		VkDescriptorSet dss[2];

		VkDescriptorSetLayout dsls[2] =
		{
			m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN],
			m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN_COMPUTE]
		};

		VkDescriptorSetAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pSetLayouts = dsls;
		alloc.descriptorSetCount = 2;
		alloc.descriptorPool = m_dp_pools[DSL_TYPE_TERRAIN].use_dp(t->dp_index);
		assert(vkAllocateDescriptorSets(m_base.device, &alloc, dss) == VK_SUCCESS);

		t->ds = dss[0];
		t->request_ds = dss[1];
	}

	//update dss
	{
		VkDescriptorBufferInfo request_buffer = {};
		request_buffer.buffer = t->request_data_buffer;
		request_buffer.offset = 0;
		request_buffer.range = sizeof(terrain_request_data);

		VkDescriptorBufferInfo data_buffer = {};
		data_buffer.buffer = t->data_buffer;
		data_buffer.offset = 0;
		data_buffer.range = sizeof(terrain_data);

		VkDescriptorImageInfo tex = {};
		tex.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		tex.imageView = t->tex.view;
		tex.sampler = t->tex.sampler;

		VkWriteDescriptorSet w[3] = {};
		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = t->request_ds;
		w[0].pBufferInfo = &request_buffer;

		w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[1].descriptorCount = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[1].dstArrayElement = 0;
		w[1].dstBinding = 0;
		w[1].dstSet = t->ds;
		w[1].pBufferInfo = &data_buffer;

		w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[2].descriptorCount = 1;
		w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[2].dstArrayElement = 0;
		w[2].dstBinding = 1;
		w[2].dstSet = t->ds;
		w[2].pImageInfo = &tex;

		/*w[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[3].descriptorCount = 1;
		w[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		w[3].dstArrayElement = 0;
		w[3].dstBinding = 1;
		w[3].dstSet = t.request_ds;
		w[3].pTexelBufferView = &t.requested_mip_levels_view;

		w[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[4].descriptorCount = 1;
		w[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		w[4].dstArrayElement = 0;
		w[4].dstBinding = 2;
		w[4].dstSet = t.ds;
		w[4].pTexelBufferView = &t.current_mip_levels_view;*/

		vkUpdateDescriptorSets(m_base.device, 3, w, 0, nullptr);
	}
}