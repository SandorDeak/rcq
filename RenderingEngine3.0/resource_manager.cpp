#include "resource_manager.h"

using namespace rcq;

resource_manager* resource_manager::m_instance = nullptr;

resource_manager::resource_manager(const base_info& info) : m_base(info)
{
	m_current_build_task_p.reset(new build_task_package);
	m_should_end_build = false;
	m_should_end_destroy = false;
	m_build_thread = std::thread(std::bind(&resource_manager::build_loop, this));
	m_destroy_thread = std::thread(std::bind(&resource_manager::destroy_loop, this));
}


resource_manager::~resource_manager()
{
	m_should_end_build = true;
	m_build_thread.join();
	m_should_end_destroy = true;
	m_destroy_thread.join();

	//checking that resources was destroyed properly
	check_resource_leak(std::make_index_sequence<RESOURCE_TYPE_COUNT>());
}

void resource_manager::init(const base_info& info)
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("resource manager is already initialised!");
	}
	m_instance = new resource_manager(info);
}

void resource_manager::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy resource manager, it doesn't exist!");
	}

	delete m_instance;
}

void resource_manager::build_loop()
{
	while (!m_should_end_build || !m_build_task_p_queue.empty())
	{
		if (!m_build_task_p_queue.empty())
		{
			std::unique_ptr<build_task_package> task_p;
			{
				std::lock_guard<std::mutex> lock(m_build_task_p_queue_mutex);
				task_p = std::move(m_build_task_p_queue.front());
				m_build_task_p_queue.pop();
			}
			do_build_tasks(*task_p.get(), std::make_index_sequence<RESOURCE_TYPE_COUNT>());
		}
	}
}

void rcq::resource_manager::process_build_package(build_package&& package)
{

	create_build_tasks(package, std::make_index_sequence<RESOURCE_TYPE_COUNT>());

	{
		std::lock_guard<std::mutex> lock(m_build_task_p_queue_mutex);
		m_build_task_p_queue.push(std::move(m_current_build_task_p));
	}

	m_current_build_task_p.reset(new build_task_package);
}

void resource_manager::destroy_loop()
{

	while (!m_should_end_destroy || !m_destroy_p_queue.empty())
	{
		bool should_check_pending_destroys = false;
		for (uint32_t i = 0; i < RESOURCE_TYPE_COUNT; ++i)
		{
			if (!m_pending_destroys[i].empty())
				should_check_pending_destroys = true;
		}
		if (should_check_pending_destroys) //happens only if invalid id is given, or 
		{
			std::lock_guard<std::mutex> lock_ready(m_resources_ready_mutex);
			std::lock_guard<std::mutex> lock_proc(m_resources_proc_mutex);
			check_pending_destroys(std::make_index_sequence<RESOURCE_TYPE_COUNT>());
		}
		if (!m_destroy_p_queue.empty())
		{
			std::unique_ptr<destroy_package> package;
			{
				std::lock_guard<std::mutex> lock_queue(m_destroy_p_queue_mutex);
				package = std::move(m_destroy_p_queue.front());
				m_destroy_p_queue.pop();
			}
			std::lock_guard<std::mutex> lock_ready(m_resources_ready_mutex);
			if (package->destroy_confirmation.has_value())
				package->destroy_confirmation.value().get();
			process_destroy_package(package->ids, std::make_index_sequence<RESOURCE_TYPE_COUNT>());
			
		}
		
		destroy_destroyables(std::make_index_sequence<RESOURCE_TYPE_COUNT>());

	}
}

template<>
mesh resource_manager::build<RESOURCE_TYPE_MESH>(std::string filename, bool calc_tb)
{
	return mesh();
}
template<>
material resource_manager::build<RESOURCE_TYPE_MAT>(material_data data, texfiles files, MAT_TYPE type)
{
	return material();
}
template<>
transform resource_manager::build<RESOURCE_TYPE_TR>(transform_data data, USAGE usage)
{
	return transform();
}

template<>
void resource_manager::destroy(mesh&& _mesh)
{

}

template<>
void resource_manager::destroy(material&& _mesh)
{

}

template<>
void resource_manager::destroy(transform&& _mesh)
{

}

/*template<>
void resource_manager::destroy<RESOURCE_TYPE_MESH>(unique_id id)
{

}
template<>
void resource_manager::destroy_tr(unique_id id)
{

}*/

/*
texture rcq::resource_manager::create_depth( int width, int height, int layer_count)
{
	texture depth;

	VkFormat depth_format = find_depth_format(m_base.physical_device);

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.arrayLayers = layer_count;
	image_info.extent.height = height;
	image_info.extent.width = width;
	image_info.extent.depth = 1;
	image_info.format = depth_format;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.mipLevels = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	if (vkCreateImage(m_base.device, &image_info, host_memory_manager, &depth.image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create depth image!");
	}

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(m_base.device, depth.image, &memory_requirements);

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type(m_base.physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(m_base.device, &alloc_info, host_memory_manager, &depth.memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate depth image memory!");
	}

	vkBindImageMemory(m_base.device, depth.image, depth.memory, 0);

	VkImageViewCreateInfo view_create_info = {};
	view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_create_info.format = depth_format;
	view_create_info.image = depth.image;
	view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view_create_info.subresourceRange.baseArrayLayer = 0;
	view_create_info.subresourceRange.baseMipLevel = 0;
	view_create_info.subresourceRange.layerCount = layer_count;
	view_create_info.subresourceRange.levelCount = 1;

	if (vkCreateImageView(m_base.device, &view_create_info, host_memory_manager, &depth.view) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create depth image view!");
	}

	VkCommandBuffer command_buffer = begin_single_time_command(m_base.device, m_command_pool);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_depth_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (depth_format == VK_FORMAT_D24_UNORM_S8_UINT || depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT)
		barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	vkCmdPipelineBarrier(command_buffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
	end_singel_time_command_buffer(m_info.device, m_command_pool, m_info.graphics_queue, command_buffer);
}*/

void rcq::resource_manager::destroy_texture(const texture & tex)
{
	vkDestroyImageView(m_base.device, tex.view, host_memory_manager);
	vkDestroyImage(m_base.device, tex.image, host_memory_manager);
	vkFreeMemory(m_base.device, tex.memory, host_memory_manager);
}