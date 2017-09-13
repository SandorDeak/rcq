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
	while (!m_should_end)
	{
		if (!m_task_p_queue.empty())
		{
			std::unique_ptr<resource_manager_task_package> task_p;
			{
				std::lock_guard<std::mutex> lock(m_task_p_queue_mutex);
				task_p = std::move(m_task_p_queue.front());
				m_task_p_queue.pop();
			}
			do_tasks(*task_p.get());
		}
	}
}

void rcq::resource_manager::do_tasks(resource_manager_task_package & package)
{
	for (auto& build_mat_t : package.build_mat_tasks)
		build_mat_t();
	for (auto& build_mesh_t : package.build_mesh_tasks)
		build_mesh_t();
	for (auto& build_tr_t : package.build_tr_tasks)
		build_tr_t();
	for (auto& destroy_mat_t : package.destroy_mat_tasks)
		destroy_mat_t();
	for (auto& destroy_mesh_t : package.destroy_mesh_tasks)
		destroy_mesh_t();
	for (auto& destroy_tr_t : package.destroy_tr_tasks)
		destroy_tr_t();
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

		task_p->build_mat_tasks.push_back(std::move(task));

	}
	for (auto& build_mesh : package.build_mesh)
	{
		build_mesh_task task(std::bind(&resource_manager::build_mesh, this,
			std::move(std::get<BUILD_MESH_INFO_FILENAME>(build_mesh)), std::get<BUILD_MESH_INFO_CALC_TB>(build_mesh)));
		if (!m_meshes_proc.insert_or_assign(std::get<BUILD_MESH_INFO_MESH_ID>(build_mesh), task.get_future()).second)
			throw std::runtime_error("unique id conflict!");

		task_p->build_mesh_tasks.push_back(std::move(task));
	}
	for (auto& build_tr : package.build_tr)
	{
		build_tr_task task(std::bind(&resource_manager::build_transform, this,
			std::move(std::get<BUILD_TR_INFO_TR_DATA>(build_tr)), std::get<BUILD_TR_INFO_USAGE>(build_tr)));
		if (!m_tr_proc.insert_or_assign(std::get<BUILD_TR_INFO_TR_ID>(build_tr), task.get_future()).second)
			throw std::runtime_error("unique id conflict!");

		task_p->build_tr_tasks.push_back(std::move(task));
	}
	for (auto& destroy_mat : package.destroy_mat)
	{
		task_p->destroy_mat_tasks.emplace_back(std::bind(&resource_manager::destroy_material, this, destroy_mat));
	}
	for (auto& destroy_mesh : package.destroy_mesh)
	{
		task_p->destroy_mesh_tasks.emplace_back(std::bind(&resource_manager::destroy_mesh, this, destroy_mesh));
	}
	for (auto& destroy_tr : package.destroy_tr)
	{
		task_p->destroy_tr_tasks.emplace_back(std::bind(&resource_manager::destroy_tr, this, destroy_tr));
	}

	std::lock_guard<std::mutex> lock(m_task_p_queue_mutex);
	m_task_p_queue.push(std::move(task_p));
}

mesh resource_manager::build_mesh(std::string filename, bool calc_tb)
{
	return mesh();
}
material resource_manager::build_material(material_data data, texfiles files, MAT_TYPE type)
{
	return material();
}
transform resource_manager::build_transform(transform_data data, USAGE usage)
{
	return transform();
}

void resource_manager::destroy_mesh(unique_id id)
{

}
void resource_manager::destroy_material(unique_id id)
{

}
void resource_manager::destroy_tr(unique_id id)
{

}

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