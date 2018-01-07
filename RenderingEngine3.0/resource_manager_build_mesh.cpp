#include "resource_manager.h"

#include "utility.h"

using namespace rcq;

template<>
void resource_manager::build<RES_TYPE_MESH>(base_resource* res, const char* build_info)
{
	const resource<RES_TYPE_MESH>::build_info* build = reinterpret_cast<const resource<RES_TYPE_MESH>::build_info*>(build_info);
	auto& mesh = *reinterpret_cast<resource<RES_TYPE_MESH>*>(res);

	vector<vertex> vertices(&m_host_memory);
	vector<uint32_t> indices(&m_host_memory);
	vector<vertex_ext> vertices_ext(&m_host_memory);
	utility::load_mesh(vertices, indices, vertices_ext, build->calc_tb, build->filename, &m_host_memory);

	mesh.size = indices.size();

	size_t vb_size = sizeof(vertex)*vertices.size();
	size_t ib_size = sizeof(uint32_t)*indices.size();
	size_t veb_size = build->calc_tb ? sizeof(vertex_ext)*vertices_ext.size() : 0;

	size_t sb_size = vb_size + ib_size + veb_size; //staging buffer size

	VkDeviceSize vb_staging = m_mappable_memory.allocate(vb_size, 1);
	VkDeviceSize ib_staging = m_mappable_memory.allocate(ib_size, 1);
	VkDeviceSize veb_staging = build->calc_tb ? m_mappable_memory.allocate(veb_size, 1) : 0;


	//create vertex buffer
	VkBufferCreateInfo vb_info = {};
	vb_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vb_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vb_info.size = vb_size;
	vb_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	assert(vkCreateBuffer(m_base.device, &vb_info, m_vk_alloc, &mesh.vb) == VK_SUCCESS);

	VkMemoryRequirements vb_mr;
	vkGetBufferMemoryRequirements(m_base.device, mesh.vb, &vb_mr);

	//create index buffer
	VkBufferCreateInfo ib_info = {};
	ib_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ib_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ib_info.size = ib_size;
	ib_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	assert(vkCreateBuffer(m_base.device, &ib_info, m_vk_alloc, &mesh.ib) == VK_SUCCESS);

	VkMemoryRequirements ib_mr;;
	vkGetBufferMemoryRequirements(m_base.device, mesh.ib, &ib_mr);

	//create vertex ext buffer
	VkMemoryRequirements veb_mr;
	veb_mr.alignment = 1;
	veb_mr.memoryTypeBits = ~0;
	veb_mr.size = 0;
	if (build->calc_tb)
	{
		VkBufferCreateInfo veb_info = {};
		veb_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		veb_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		veb_info.size = veb_size;
		veb_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		assert(vkCreateBuffer(m_base.device, &veb_info, m_vk_alloc, &mesh.veb) == VK_SUCCESS);

		vkGetBufferMemoryRequirements(m_base.device, mesh.veb, &veb_mr);

	}
	else
	{
		mesh.veb = VK_NULL_HANDLE;
	}

	//allocate buffer memory
	VkDeviceSize vb_offset = m_dl0_memory.allocate(vb_size, vb_mr.alignment);
	VkDeviceSize ib_offset = m_dl0_memory.allocate(ib_size, ib_mr.alignment);
	VkDeviceSize veb_offset = build->calc_tb ? m_dl0_memory.allocate(veb_size, veb_mr.alignment) : 0;

	/*VkDeviceSize ib_offset = calc_offset(ib_mr.alignment, vb_mr.size);
	VkDeviceSize veb_offset = calc_offset(veb_mr.alignment, ib_offset + ib_mr.size);
	VkDeviceSize size = veb_offset + veb_mr.size;

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.memoryTypeIndex = find_memory_type(m_base.physical_device, vb_mr.memoryTypeBits & ib_mr.memoryTypeBits &
	veb_mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	alloc_info.allocationSize = size;

	if (vkAllocateMemory(m_base.device, &alloc_info, m_alloc, &_mesh.memory) != VK_SUCCESS)
	{
	throw std::runtime_error("failed to allocate buffer memory!");
	}*/

	//bind buffers to memory
	vkBindBufferMemory(m_base.device, mesh.vb, m_dl0_memory.handle(), vb_offset);
	vkBindBufferMemory(m_base.device, mesh.ib, m_dl0_memory.handle(), ib_offset);
	if (build->calc_tb)
		vkBindBufferMemory(m_base.device, mesh.veb, m_dl0_memory.handle(), veb_offset);

	//fill staging buffer

	void* raw_data;
	vkMapMemory(m_base.device, m_mappable_memory.handle(), vb_staging, sb_size, 0, &raw_data);
	char* data = static_cast<char*>(raw_data);
	memcpy(data, vertices.data(), vb_size);
	vkUnmapMemory(m_base.device, m_mappable_memory.handle());

	vkMapMemory(m_base.device, m_mappable_memory.handle(), ib_staging, ib_size, 0, &raw_data);
	data = static_cast<char*>(raw_data);
	memcpy(data, indices.data(), ib_size);
	vkUnmapMemory(m_base.device, m_mappable_memory.handle());
	if (build->calc_tb)
	{
		vkMapMemory(m_base.device, m_mappable_memory.handle(), veb_staging, veb_size, 0, &raw_data);
		data = static_cast<char*>(raw_data);
		memcpy(data, vertices_ext.data(), veb_size);
		vkUnmapMemory(m_base.device, m_mappable_memory.handle());
	}

	//transfer from staging buffer
	begin_build_cb();

	VkBufferCopy copy_region;
	copy_region.dstOffset = 0;
	copy_region.size = vb_size;
	copy_region.srcOffset = vb_staging;
	vkCmdCopyBuffer(m_build_cb, m_staging_buffer, mesh.vb, 1, &copy_region);

	copy_region.dstOffset = 0;
	copy_region.size = ib_size;
	copy_region.srcOffset = ib_staging;
	vkCmdCopyBuffer(m_build_cb, m_staging_buffer, mesh.ib, 1, &copy_region);


	if (build->calc_tb)
	{
		copy_region.dstOffset = veb_offset;
		copy_region.size = veb_size;
		copy_region.srcOffset = veb_staging;
		vkCmdCopyBuffer(m_build_cb, m_staging_buffer, mesh.veb, 1, &copy_region);
	}

	end_build_cb();
	wait_for_build_fence();

	res->ready_bit.store(true, std::memory_order_release);

	m_mappable_memory.deallocate(vb_staging);
	m_mappable_memory.deallocate(ib_staging);
	if (build->calc_tb)
		m_mappable_memory.deallocate(veb_staging);
}