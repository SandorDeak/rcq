#include "resource_manager.h"

using namespace rcq;

resource_manager* resource_manager::m_instance = nullptr;

resource_manager::resource_manager(const base_info& info) : m_base(info)
{
	m_current_build_task_p.reset(new build_task_package);
	m_should_end_build = false;
	m_should_end_destroy = false;
	m_build_thread = std::thread([this]()
	{
		try
		{
			build_loop();
		}
		catch (const std::runtime_error& e)
		{
			std::cerr << e.what() << std::endl;
		}
	});

	m_destroy_thread = std::thread([this]()
	{
		try
		{
			destroy_loop();
		}
		catch (const std::runtime_error& e)
		{
			std::cerr << e.what() << std::endl;
		}
	});
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
		if (should_check_pending_destroys) //happens only if invalid id is given, or resource was never used 
		{
			std::lock_guard<std::mutex> lock_proc(m_resources_proc_mutex);
			std::lock_guard<std::mutex> lock_ready(m_resources_ready_mutex);
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
				package->destroy_confirmation.value().wait();
			process_destroy_package(package->ids, std::make_index_sequence<RESOURCE_TYPE_COUNT>());
			
		}
		
		destroy_destroyables(std::make_index_sequence<RESOURCE_TYPE_COUNT>());

	}
}

mesh resource_manager::build(const std::string& filename, bool calc_tb)
{
	return mesh();
}

material resource_manager::build(const material_data& data, const texfiles& files, MAT_TYPE type)
{
	return material();
}

transform resource_manager::build(const transform_data& data, USAGE usage)
{
	return transform();
}

memory resource_manager::build(const std::vector<VkMemoryRequirements>& requirements)
{
	return memory();
}

void resource_manager::destroy(mesh&& _mesh)
{
	vkDestroyBuffer(m_base.device, _mesh.ib, host_memory_manager);
	vkDestroyBuffer(m_base.device, _mesh.vb, host_memory_manager);
	if (_mesh.veb != VK_NULL_HANDLE)
		vkDestroyBuffer(m_base.device, _mesh.veb, host_memory_manager);

	vkFreeMemory(m_base.device, _mesh.memory, host_memory_manager);
}

void resource_manager::destroy(material&& _mat)
{
	for (size_t i = 0; i < TEX_TYPE_COUNT; ++i)
	{
		if (_mat.texs[i].view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(m_base.device, _mat.texs[i].view, host_memory_manager);
			vkDestroyImage(m_base.device, _mat.texs[i].image, host_memory_manager);
			vkFreeMemory(m_base.device, _mat.texs[i].memory, host_memory_manager);
		}
	}
	vkFreeDescriptorSets(m_base.device, /*TODO: pool*/VK_NULL_HANDLE, 1, &_mat.ds);
	//TODO free data
}

void resource_manager::destroy(transform&& _tr)
{
	//TODO: free data
}

void resource_manager::destroy(memory && _memory)
{
	for (auto mem : _memory)
		vkFreeMemory(m_base.device, mem, host_memory_manager);
}

void rcq::resource_manager::update_cam_and_tr(const std::optional<camera_data>& cam, const std::vector<update_tr_info>& trs)
{

	if (cam)
	{
		memcpy(m_tr_dynamic_buffer_begins[0], &cam.value(), sizeof(camera_data));
	}
	size_t staging_buffer_offset = 0;
	bool has_static = false;

	for (const auto& tr_info : trs) //TODO: this should be optimized
	{
		auto tr = std::get<RESOURCE_TYPE_TR>(m_resources_ready)[std::get<UPDATE_TR_INFO_TR_ID>(tr_info)];
		if (tr.usage==USAGE_DYNAMIC)
			memcpy(tr.data, &std::get<UPDATE_TR_INFO_TR_DATA>(tr_info), sizeof(transform_data));
		else
			has_static = true;
	}

	if (has_static)
	{
		VkCommandBuffer cb=VK_NULL_HANDLE;
		//TODO: begin cb
		VkBufferCopy region = {};
		region.size = sizeof(transform_data);
		region.dstOffset = 0;
		region.srcOffset = 0;

		for (const auto& tr_info : trs)
		{
			auto tr= std::get<RESOURCE_TYPE_TR>(m_resources_ready)[std::get<UPDATE_TR_INFO_TR_ID>(tr_info)];
			if (tr.usage == USAGE_STATIC)
			{
				memcpy(m_tr_static_staging_buffer_begin + region.srcOffset, 
					&std::get<UPDATE_TR_INFO_TR_DATA>(tr_info), sizeof(transform_data));
				vkCmdCopyBuffer(cb, m_tr_static_staging_buffer, tr.buffer, 1, &region);
				region.srcOffset += sizeof(transform_data);
			}
		}

		//TODO: end cb
	}
}
