#include "terrain_manager.h"
#include "host_memory.h"
#include "device_memory.h"

using namespace rcq;

terrain_manager::terrain_manager(const base_info& base, memory_resource* host_memory_resource,
	device_memory_resource* device_memory_res) :
	m_base(base)

{}

terrain_manager::~terrain_manager()
{
}

void terrain_manager::create_memory_resources_and_containers()
{
	m_host_memory_res.init(64 * 1024 * 1024, host_memory::instance()->resource()->max_alignment(), host_memory::instance()->resource());
	m_vk_alloc.init(&m_host_memory_res);

	m_page_pool.init(64 * 64 * sizeof(glm::vec4), 256, device_memory::instance()->resource(), &m_host_memory_res);
	m_mappable_memory_resource.init(m_base.device, device_memory::instance()->find_memory_type(~0,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), false, &m_vk_alloc);

	m_request_queue.init(&m_host_memory_res);
	m_request_queue.init_buffer();
	m_result_queue.init(&m_host_memory_res);
	m_result_queue.init_buffer();
}

void terrain_manager::poll_requests()
{
	//std::lock_guard<std::mutex> lock(m_request_queue_mutex);
	for (uint32_t i = 0; i < m_request_data->request_count; ++i)
	{
		*m_request_queue.create_back() = m_request_data->requests[i];
	}
	m_request_queue.commit();
	m_request_data->request_count = 0;
}
void terrain_manager::poll_results()
{
	//std::lock_guard<std::mutex> lock(m_result_queue_mutex);
	while (!m_result_queue.empty())
	{
		uint32_t result = *m_result_queue.front();
		m_result_queue.pop();

		glm::uvec2 tile_id =
		{
			result & (MAX_TILE_COUNT - 1u),
			(result >> MAX_TILE_COUNT_LOG2) & (MAX_TILE_COUNT - 1u)
		};

		if (result >> 31)
			m_current_mip_levels[tile_id.x + tile_id.y*m_tile_count.x] -= 1.f;
		else
			m_current_mip_levels[tile_id.x + tile_id.y*m_tile_count.x] += 1.f;
	}
}

void terrain_manager::loop()
{
	while (!m_should_end)
	{
		while (m_request_queue.empty());
		uint32_t request;
		{
			//std::lock_guard<std::mutex> lock(m_request_queue_mutex);
			request = *m_request_queue.front();
			m_request_queue.pop();
		}

		glm::uvec2 tile_id =
		{
			request & (MAX_TILE_COUNT - 1u),
			(request >> MAX_TILE_COUNT_LOG2) & (MAX_TILE_COUNT - 1u)
		};
		if (request >> 31)
		{
			decrease_min_mip_level(tile_id);
		}
		else
		{
			increase_min_mip_level(tile_id);
		}

		*m_result_queue.create_back() = request;
		m_result_queue.commit();
	}

}

void terrain_manager::decrease_min_mip_level(const glm::uvec2& tile_id)
{
	std::cout << "decrease request received: " << tile_id.x << ' ' << tile_id.y << '\n';
	uint32_t new_mip_level = static_cast<uint32_t>(m_current_mip_levels[tile_id.x + tile_id.y*m_tile_count.x]) - 1u;
	auto file = m_terrain->tex.files[new_mip_level];
	glm::uvec2 tile_size_in_pages = m_tile_size_in_pages*static_cast<uint32_t>(powf(0.5f, static_cast<float>(new_mip_level)));
	glm::uvec2 page_size = { 64, 64 };
	glm::uvec2 file_size = m_tile_count*tile_size_in_pages*page_size;
	glm::uvec2 tile_offset2D_in_file = tile_id*tile_size_in_pages*page_size;

	auto& pages = m_pages[new_mip_level][tile_id.x][tile_id.y];
	pages.resize(tile_size_in_pages.x*tile_size_in_pages.y);

	size_t tile_offset_in_bytes = (tile_offset2D_in_file.y*file_size.x + tile_offset2D_in_file.x) * sizeof(glm::vec4);
	size_t page_index = 0;

	char* pages_staging = m_pages_staging;
	for (uint32_t i = 0; i < tile_size_in_pages.x; ++i)
	{
		for (uint32_t j = 0; j < tile_size_in_pages.y; ++j)
		{
			pages[page_index] = m_page_pool.allocate(m_page_size, 1);

			uint64_t page_offset_in_bytes = tile_offset_in_bytes
				+ (file_size.x*page_size.y*j + i*page_size.x) * sizeof(glm::vec4);

			for (uint32_t k = 0; k < page_size.y; ++k)
			{
				size_t offset_in_bytes = page_offset_in_bytes + k*file_size.x * sizeof(glm::vec4);
				file->seekg(offset_in_bytes);
				file->read(pages_staging, page_size.x * sizeof(glm::vec4));
				pages_staging += (page_size.x * sizeof(glm::vec4));
			}
			++page_index;
		}
	}

	//bind pages to virtual texture
	uint32_t page_count = tile_size_in_pages.x*tile_size_in_pages.y;
	vector<VkSparseImageMemoryBind> image_binds(&m_host_memory_res, page_count);

	page_index = 0;
	for (uint32_t i = 0; i < tile_size_in_pages.x; ++i)
	{
		for (uint32_t j = 0; j < tile_size_in_pages.y; ++j)
		{
			auto& b = image_binds[page_index];
			b.extent = { page_size.x, page_size.y, 1 };
			b.offset = {
				static_cast<int32_t>(tile_offset2D_in_file.x + i*page_size.x),
				static_cast<int32_t>(tile_offset2D_in_file.y + j*page_size.y),
				0 };
			b.memory = m_page_pool.handle();
			b.memoryOffset = pages[page_index];
			b.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresource.arrayLayer = 0;
			b.subresource.mipLevel = new_mip_level;

			++page_index;
		}
	}

	VkSparseImageMemoryBindInfo image_bind_info = {};
	image_bind_info.bindCount = page_count;
	image_bind_info.image = m_terrain->tex.image;
	image_bind_info.pBinds = image_binds.data();

	VkBindSparseInfo bind_info = {};
	bind_info.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
	bind_info.imageBindCount = 1;
	bind_info.pImageBinds = &image_bind_info;
	bind_info.signalSemaphoreCount = 1;
	bind_info.pSignalSemaphores = &m_binding_finished_s;


	//std::lock_guard<std::mutex> lock(m_base.graphics_queue_mutex);
	vkQueueBindSparse(m_base.transfer_queue, 1, &bind_info, VK_NULL_HANDLE);


	//copy from staging buffers
	vector<VkBufferImageCopy> regions(&m_host_memory_res, page_count);
	page_index = 0;
	for (uint32_t i = 0; i < tile_size_in_pages.x; ++i)
	{
		for (uint32_t j = 0; j < tile_size_in_pages.y; ++j)
		{
			auto& r = regions[page_index];
			r.bufferOffset = page_index*m_page_size+m_pages_staging_offset;
			r.imageOffset = {
				static_cast<int32_t>(tile_offset2D_in_file.x + i*page_size.x),
				static_cast<int32_t>(tile_offset2D_in_file.y + j*page_size.y),
				0 };
			r.imageExtent = { page_size.x, page_size.y, 1 };
			r.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			r.imageSubresource.baseArrayLayer = 0;
			r.imageSubresource.layerCount = 1;
			r.imageSubresource.mipLevel = new_mip_level;

			++page_index;
		}
	}

	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	assert(vkBeginCommandBuffer(m_cb, &begin) == VK_SUCCESS);

	vkCmdCopyBufferToImage(m_cb, m_mapped_buffer, m_terrain->tex.image,
		VK_IMAGE_LAYOUT_GENERAL, page_count, regions.data());

	assert(vkEndCommandBuffer(m_cb) == VK_SUCCESS);

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &m_cb;
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	submit.waitSemaphoreCount = 1;
	submit.pWaitDstStageMask = &wait_stage;
	submit.pWaitSemaphores = &m_binding_finished_s;
	//std::lock_guard<std::mutex> lock(m_base.graphics_queue_mutex);
	assert(vkQueueSubmit(m_base.graphics_queue, 1, &submit, m_copy_finished_f) == VK_SUCCESS);
	vkWaitForFences(m_base.device, 1, &m_copy_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_base.device, 1, &m_copy_finished_f);
}


void terrain_manager::increase_min_mip_level(const glm::uvec2& tile_id)
{
	std::cout << "increase request received: " << tile_id.x << ' ' << tile_id.y << '\n';
	uint32_t destroy_mip_level = static_cast<uint32_t>(m_current_mip_levels[tile_id.x + tile_id.y*m_tile_count.x]);
	auto& pages = m_pages[destroy_mip_level][tile_id.x][tile_id.y];

	for (uint32_t i = 0; i < pages.size(); ++i)
		m_page_pool.deallocate(pages[i]);

	pages.clear();
}

void terrain_manager::init(const glm::uvec2& tile_count, const glm::uvec2& tile_size, uint32_t mip_level_count, VkCommandPool cp)
{
	m_tile_count = tile_count;
	m_tile_size = tile_size;
	m_tile_size_in_pages = tile_size / 64u;

	uint64_t size = sizeof(request_data) + 2 * 4 * tile_count.x*tile_count.y+
		m_tile_size_in_pages.x*m_tile_size_in_pages.y*64*64*sizeof(glm::vec4);

	uint64_t requested_mip_levels_offset;
	uint64_t current_mip_levels_offset;
	m_page_size = 64 * 64 * sizeof(glm::vec4);

	//create mapped buffer
	{
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer.size = size;
		buffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
			VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

		assert(vkCreateBuffer(m_base.device, &buffer, m_vk_alloc, &m_mapped_buffer) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, m_mapped_buffer, &mr);
		uint64_t alignment = 256; // mr.alignment < alignof(request_data) ? alignof(request_data) : mr.alignment;
		m_mapped_offset = m_mappable_memory_resource.allocate(mr.size, alignment);
		requested_mip_levels_offset = m_mapped_offset + sizeof(request_data);
		current_mip_levels_offset = requested_mip_levels_offset + 4 * tile_count.x*tile_count.y;
		vkBindBufferMemory(m_base.device, m_mapped_buffer, m_mappable_memory_resource.handle(), m_mapped_offset);
		char* data = reinterpret_cast<char*>(m_mappable_memory_resource.map(m_mapped_offset, size));

		m_request_data = reinterpret_cast<request_data*>(data);
		m_requested_mip_levels = reinterpret_cast<float*>(data + sizeof(request_data));
		m_current_mip_levels = reinterpret_cast<float*>(data + sizeof(request_data) + 4 * tile_count.x*tile_count.y);
		m_pages_staging = data + sizeof(request_data) + 2 * 4 * tile_count.x*tile_count.y;
		m_pages_staging_offset=m_mapped_offset+ sizeof(request_data) + 2 * 4 * tile_count.x*tile_count.y;
		m_request_data->request_count = 0;
	}

	//create requested mip levels view
	{
		VkBufferViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		view.buffer = m_mapped_buffer;
		view.format = VK_FORMAT_R32_SFLOAT;
		view.offset = requested_mip_levels_offset;
		view.range = 4 * tile_count.x*tile_count.y;

		assert(vkCreateBufferView(m_base.device, &view, m_vk_alloc, &m_requested_mip_levels_view) == VK_SUCCESS);
	}

	//create current mip levels view
	{
		VkBufferViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		view.buffer = m_mapped_buffer;
		view.format = VK_FORMAT_R32_SFLOAT;
		view.offset = current_mip_levels_offset;
		view.range = 4 * tile_count.x*tile_count.y;

		assert(vkCreateBufferView(m_base.device, &view, m_vk_alloc, &m_current_mip_levels_view) == VK_SUCCESS);
	}

	m_cp = cp;

	VkSemaphoreCreateInfo s = {};
	s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	assert(vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &m_binding_finished_s) == VK_SUCCESS);

	VkFenceCreateInfo f = {};
	f.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	assert(vkCreateFence(m_base.device, &f, m_vk_alloc, &m_copy_finished_f) == VK_SUCCESS);

	VkCommandBufferAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc.commandBufferCount = 1;
	alloc.commandPool = m_cp;
	alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_cb) == VK_SUCCESS);

	m_pages.resize(mip_level_count);
	for (auto& v : m_pages)
	{
		v.resize(tile_count.x);
		for (auto& w : v)
			w.resize(tile_count.y);
	}

	m_should_end = false;
	m_thread = std::thread([this]()
	{
		loop();
	});
}

void terrain_manager::destroy()
{
	m_should_end = true;
	m_thread.join();
	vkDestroyFence(m_base.device, m_copy_finished_f, m_vk_alloc);
	vkDestroySemaphore(m_base.device, m_binding_finished_s, m_vk_alloc);
	vkFreeCommandBuffers(m_base.device, m_cp, 1, &m_cb);
}
