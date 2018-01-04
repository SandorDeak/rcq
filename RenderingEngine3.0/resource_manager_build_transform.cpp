#include "resource_manager.h"

using namespace rcq;

template<>
void resource_manager::build<RES_TYPE_TR>(base_resource* res, const char* build_info)
{

	auto& tr = *reinterpret_cast<resource<RES_TYPE_TR>*>(res->data);
	const auto& build = *reinterpret_cast<const resource<RES_TYPE_TR>::build_info*>(build_info);

	//create buffer
	{
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer.size = sizeof(resource<RES_TYPE_TR>::data);
		buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		assert(vkCreateBuffer(m_base.device, &buffer, m_vk_alloc, &tr.data_buffer) == VK_SUCCESS);
	}

	//allocate memory
	{
		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, tr.data_buffer, &mr);

		tr.data_offset = m_device_memory.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, tr.data_buffer, m_device_memory.handle(), tr.data_offset);
	}

	//allocate and fill staging memory
	uint64_t staging_buffer_offset = m_mappable_memory.allocate(sizeof(resource<RES_TYPE_TR>::data), alignof(resource<RES_TYPE_TR>::data));
	resource<RES_TYPE_TR>::data* tr_data = reinterpret_cast<resource<RES_TYPE_TR>::data*>(m_mappable_memory.map(staging_buffer_offset,
		sizeof(resource<RES_TYPE_TR>::data)));

	tr_data->model = build.model;
	tr_data->scale = build.scale;
	tr_data->tex_scale = build.tex_scale;

	m_mappable_memory.unmap();

	//copy from staging buffer
	{
		begin_build_cb();

		VkBufferCopy region = {};
		region.dstOffset = 0;
		region.size = sizeof(resource<RES_TYPE_TR>::data);
		region.srcOffset = staging_buffer_offset;

		vkCmdCopyBuffer(m_build_cb, m_staging_buffer, tr.data_buffer, 1, &region);

		end_build_cb();
	}

	//allocate descriptor set
	{
		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &m_dsls[DSL_TYPE_TR];

		alloc_info.descriptorPool = m_dp_pools[DSL_TYPE_TR].use_dp(tr.dp_index);
		assert(vkAllocateDescriptorSets(m_base.device, &alloc_info, &tr.ds) == VK_SUCCESS);
	}

	//update descriptor set
	VkDescriptorBufferInfo buffer_info;
	buffer_info.buffer = tr.data_buffer;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(resource<RES_TYPE_TR>::data);

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.dstArrayElement = 0;
	write.dstBinding = 0;
	write.dstSet = tr.ds;
	write.pBufferInfo = &buffer_info;

	vkUpdateDescriptorSets(m_base.device, 1, &write, 0, nullptr);

	wait_for_build_fence();
	res->ready_bit.store(true, std::memory_order_release);

	m_mappable_memory.deallocate(staging_buffer_offset);
}