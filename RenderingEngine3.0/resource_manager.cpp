#include "resource_manager.h"

using namespace rcq;

resource_manager* resource_manager::m_instance = nullptr;

resource_manager::resource_manager(const base_info& info) : m_base(info)
{
	//should init something?
	m_should_end = false;
	m_working_thread = std::thread(std::bind(&resource_manager::work, this));
}


resource_manager::~resource_manager()
{
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

void resource_manager::work()
{
	
}

void resource_manager::build_mesh_async(const build_mesh_info& info)
{

}
void resource_manager::build_material_async(const build_mat_info& info)
{

}
void resource_manager::build_transform_async(const build_tr_info& info)
{

}

void resource_manager::destroy_mesh_async(unique_id id)
{

}
void resource_manager::destroy_material_async(unique_id id)
{

}
void resource_manager::destroy_tr_async(const std::pair<unique_id, USAGE>& info)
{

}

void rcq::resource_manager::process_package_async(resource_manager_package && package)
{
	auto task_p = std::make_unique<resource_manager_task_package>();
	for (auto& build_mat : package.build_mat)
	{
		build_mat_task task(std::bind(&resource_manager::build_material, this,
			std::move(std::get<BUILD_MAT_INFO_MAT_DATA>(build_mat)), std::move(std::get<BUILD_MAT_INFO_TEXINFOS>(build_mat)),
			std::move(std::get<BUILD_MAT_INFO_MAT_TYPE>(build_mat))));

		if (!m_mats_proc.insert_or_assign(std::get<BUILD_MAT_INFO_MAT_ID>(build_mat), task.get_future()).second)
			throw std::runtime_error("unique id conflict!");

		task_p->build_mat_tasks.emplace_back(std::move(task));
	}

	//do for others
}

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
}

void rcq::resource_manager::destroy_texture(const texture & tex)
{
	vkDestroyImageView(m_base.device, tex.view, host_memory_manager);
	vkDestroyImage(m_base.device, tex.image, host_memory_manager);
	vkFreeMemory(m_base.device, tex.memory, host_memory_manager);
}