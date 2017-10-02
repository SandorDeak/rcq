#include "resource_manager.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "device_memory.h"

using namespace rcq;

resource_manager* resource_manager::m_instance = nullptr;

void load_mesh(const std::string& file_name, std::vector<vertex>& vertices, std::vector<uint32_t>& indices,
	bool make_vertex_ext, std::vector<vertex_ext>& vertices_ext);

resource_manager::resource_manager(const base_info& info) : m_base(info)
{
	m_current_build_task_p.reset(new build_task_package);

	create_samplers();
	create_descriptor_set_layouts();
	create_command_pool();

	m_should_end_build = false;
	m_should_end_destroy = false;
	m_build_thread = std::thread([this]()
	{
		/*try
		{*/
			build_loop();
		/*}

		catch (const std::runtime_error& e)
		{
			std::cerr << e.what() << std::endl;
		}*/
	});

	m_destroy_thread = std::thread([this]()
	{
		/*try
		{*/
			destroy_loop();
		/*}
		catch (const std::runtime_error& e)
		{
			std::cerr << e.what() << std::endl;
		}*/
	});

	create_staging_buffers();

}


resource_manager::~resource_manager()
{
	m_should_end_build = true;
	m_build_thread.join();

	vkUnmapMemory(m_base.device, m_single_cell_sb_mem);
	vkUnmapMemory(m_base.device, m_sb_mem);
	vkDestroyBuffer(m_base.device, m_single_cell_sb, host_memory_manager);
	vkDestroyBuffer(m_base.device, m_sb, host_memory_manager);
	auto destroy = std::make_unique<destroy_package>();
	destroy->ids[RESOURCE_TYPE_MEMORY].push_back(~0);
	push_destroy_package(std::move(destroy));
	
	m_should_end_destroy = true;
	m_destroy_thread.join();	

	for (auto& sampler : m_samplers)
		vkDestroySampler(m_base.device, sampler, host_memory_manager);
	for (auto& dsl : m_dsls)
		vkDestroyDescriptorSetLayout(m_base.device, dsl, host_memory_manager);
	for (auto& dpp : m_dpps)
	{
		for (auto& dp : dpp.pools)
			vkDestroyDescriptorPool(m_base.device, dp, host_memory_manager);
	}
	vkDestroyCommandPool(m_base.device, m_cp_build, host_memory_manager);
	vkDestroyCommandPool(m_base.device, m_cp_update, host_memory_manager);

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
	bool should_check_pending_destroys = false;

	while (!m_should_end_destroy || !m_destroy_p_queue.empty() || should_check_pending_destroys)
	{
		
		if (should_check_pending_destroys) //happens only if invalid id is given, or resource was never used 
		{
			std::lock_guard<std::mutex> lock_proc(m_resources_proc_mutex);
			std::lock_guard<std::mutex> lock_ready(m_resources_ready_mutex);
			check_pending_destroys(std::make_index_sequence<RESOURCE_TYPE_COUNT>());
		}
		should_check_pending_destroys = false;
		for (uint32_t i = 0; i < RESOURCE_TYPE_COUNT; ++i)
		{
			if (!m_pending_destroys[i].empty())
				should_check_pending_destroys = true;
		}

		if (!m_destroy_p_queue.empty())
		{
			std::unique_ptr<destroy_package> package;
			{
				std::lock_guard<std::mutex> lock_queue(m_destroy_p_queue_mutex);
				package = std::move(m_destroy_p_queue.front());
				m_destroy_p_queue.pop();
			}
			if (package->destroy_confirmation.has_value())
				package->destroy_confirmation.value().wait();
			std::lock_guard<std::mutex> lock_ready(m_resources_ready_mutex);
			process_destroy_package(package->ids, std::make_index_sequence<RESOURCE_TYPE_COUNT>());
			
		}
		
		destroy_destroyables(std::make_index_sequence<RESOURCE_TYPE_COUNT>());

	}
}

void load_mesh(const std::string& file_name, std::vector<vertex>& vertices, std::vector<uint32_t>& indices,
	bool make_vertex_ext, std::vector<vertex_ext>& vertices_ext)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, file_name.c_str()))
	{
		throw std::runtime_error(err);
	}

	std::unordered_map<vertex, uint32_t> unique_vertices = {};

	for (const auto& shape : shapes)
	{
		size_t count = 0;
		for (const auto& index : shape.mesh.indices)
		{
			vertex v = {};
			v.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			v.normal = {
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]
			};

			if (!attrib.texcoords.empty())
			{
				v.tex_coord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1 - attrib.texcoords[2 * index.texcoord_index + 1]
				};
			}

			if (unique_vertices.count(v) == 0)
			{
				unique_vertices[v] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(std::move(v));
				if (make_vertex_ext)
				{
					vertex_ext vertex_ext_templ = { glm::vec3(0.f), glm::vec3(0.f) };
					vertices_ext.push_back(std::move(vertex_ext_templ));
				}
			}
			indices.push_back(unique_vertices[v]);
			++count;

			if (count == 3 && make_vertex_ext)
			{
				glm::vec2 t0 = vertices[*indices.rbegin()].tex_coord;
				glm::vec2 t1 = vertices[*(indices.rbegin() + 1)].tex_coord;
				glm::vec2 t2 = vertices[*(indices.rbegin() + 2)].tex_coord;

				glm::vec3 p0 = vertices[*indices.rbegin()].pos;
				glm::vec3 p1 = vertices[*(indices.rbegin() + 1)].pos;
				glm::vec3 p2 = vertices[*(indices.rbegin() + 2)].pos;

				glm::mat2x3 tb = glm::mat2x3(p1 - p0, p2 - p0)*glm::inverse(glm::mat2(t1 - t0, t2 - t0));

				for (auto it = indices.rbegin(); it != indices.rbegin() + 3; ++it)
				{
					vertices_ext[*it].tangent += tb[0];
					vertices_ext[*it].bitangent += tb[1];
				}
				count = 0;
			}
		}
	}

	for (size_t i = 0; i<vertices_ext.size(); ++i)
	{
		//vertex_ext.tangent = glm::normalize(vertex_ext.tangent);
		//vertex_ext.bitangent = glm::normalize(vertex_ext.bitangent);
		glm::vec3 n = vertices[i].normal;
		glm::vec3 t = vertices_ext[i].tangent;
		t = glm::normalize(t - glm::dot(t, n)*n);
		vertices_ext[i].tangent = t;
		vertices_ext[i].bitangent = glm::cross(t, n);
	}
}

mesh resource_manager::build(const std::string& filename, bool calc_tb)
{

	std::vector<vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<vertex_ext> vertices_ext;
	load_mesh(filename, vertices, indices, calc_tb, vertices_ext);

	mesh _mesh;
	_mesh.size = indices.size();

	size_t vb_size = sizeof(vertex)*vertices.size();
	size_t ib_size = sizeof(uint32_t)*indices.size();
	size_t veb_size = calc_tb? sizeof(vertex_ext)*vertices_ext.size():0;

	size_t sb_size = vb_size + ib_size + veb_size; //staging buffer size

	//create vertex buffer
	VkBufferCreateInfo vb_info = {};
	vb_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vb_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vb_info.size = vb_size;
	vb_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if (vkCreateBuffer(m_base.device, &vb_info, host_memory_manager, &_mesh.vb) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create vertex buffer!");
	}

	VkMemoryRequirements vb_mr;
	vkGetBufferMemoryRequirements(m_base.device, _mesh.vb, &vb_mr);

	//create index buffer
	VkBufferCreateInfo ib_info = {};
	ib_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ib_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ib_info.size = ib_size;
	ib_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if (vkCreateBuffer(m_base.device, &ib_info, host_memory_manager, &_mesh.ib) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create index buffer!");
	}

	VkMemoryRequirements ib_mr;;
	vkGetBufferMemoryRequirements(m_base.device, _mesh.ib, &ib_mr);

	//create vertex ext buffer
	VkMemoryRequirements veb_mr;
	veb_mr.alignment = 1;
	veb_mr.memoryTypeBits = ~0;
	veb_mr.size = 0;
	if (calc_tb)
	{
		VkBufferCreateInfo veb_info = {};
		veb_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		veb_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		veb_info.size = veb_size;
		veb_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		if (vkCreateBuffer(m_base.device, &veb_info, host_memory_manager, &_mesh.veb) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create vertex ext buffer!");
		}

		vkGetBufferMemoryRequirements(m_base.device, _mesh.veb, &veb_mr);

	}
	else
	{
		_mesh.veb = VK_NULL_HANDLE;
	}

	//allocate buffer memory
	VkDeviceSize ib_offset = calc_offset(ib_mr.alignment, vb_mr.size);
	VkDeviceSize veb_offset = calc_offset(veb_mr.alignment, ib_offset + ib_mr.size);
	VkDeviceSize size = veb_offset + veb_mr.size;

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.memoryTypeIndex = find_memory_type(m_base.physical_device, vb_mr.memoryTypeBits & ib_mr.memoryTypeBits &
		veb_mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	alloc_info.allocationSize = size;

	if (vkAllocateMemory(m_base.device, &alloc_info, host_memory_manager, &_mesh.memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	//bind buffers to memory
	vkBindBufferMemory(m_base.device, _mesh.vb, _mesh.memory, 0);
	vkBindBufferMemory(m_base.device, _mesh.ib, _mesh.memory, ib_offset);
	if (calc_tb)
		vkBindBufferMemory(m_base.device, _mesh.veb, _mesh.memory, veb_offset);

	//create and fill staging buffer
	VkBuffer sb;
	VkDeviceMemory sb_mem;
	create_staging_buffer(m_base, sb_size, sb, sb_mem);

	void* raw_data;
	vkMapMemory(m_base.device, sb_mem, 0, sb_size, 0, &raw_data);
	char* data = static_cast<char*>(raw_data);
	memcpy(data, vertices.data(), vb_size);
	memcpy(data + vb_size, indices.data(), ib_size);
	if (calc_tb)
		memcpy(data + vb_size + ib_size, vertices_ext.data(), veb_size);
	vkUnmapMemory(m_base.device, sb_mem);

	//transfer from staging buffer
	VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);
	VkBufferCopy copy_region = {};	
	copy_region.dstOffset = 0;
	copy_region.size = vb_size;
	copy_region.srcOffset = 0;
	vkCmdCopyBuffer(cb, sb, _mesh.vb, 1, &copy_region);

	copy_region.srcOffset = vb_size;
	copy_region.size = ib_size;
	vkCmdCopyBuffer(cb, sb, _mesh.ib, 1, &copy_region);

	if (calc_tb)
	{
		copy_region.srcOffset = vb_size + ib_size;
		copy_region.size = veb_size;
		vkCmdCopyBuffer(cb, sb, _mesh.veb, 1, &copy_region);
	}

	end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);

	//destroy staging buffer
	vkDestroyBuffer(m_base.device, sb, host_memory_manager);
	vkFreeMemory(m_base.device, sb_mem, host_memory_manager);

	return _mesh;
}

material resource_manager::build(const material_data& data, const texfiles& files, MAT_TYPE type)
{
	//load textures
	material mat;
	mat.type = type;
	uint32_t flag = 1;
	for (uint32_t i=0; i<TEX_TYPE_COUNT; ++i)
	{
		if ((flag & data.flags) && files[i]!="empty") //TODO: if empty, create an empty texture
			mat.texs[i] = load_texture(files[i]);
		else
			mat.texs[i].view = VK_NULL_HANDLE;
		flag = flag << 1;
	}

	//create material data buffer
	VkBufferCreateInfo buffer_create_info = {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_create_info.size = sizeof(material_data);
	buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (vkCreateBuffer(m_base.device, &buffer_create_info, host_memory_manager, &mat.data) != VK_SUCCESS)
		throw std::runtime_error("failed to create material data buffer!");

	//allocate memory for material data buffer
	mat.cell = device_memory::instance()->alloc_buffer_memory(USAGE_STATIC, mat.data, nullptr);

	//copy to staging buffer
	memcpy(m_single_cell_sb_data, &data, sizeof(material_data));

	//copy to material data buffer
	VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);

	VkBufferCopy region = {};
	region.dstOffset = 0;
	region.srcOffset = 0;
	region.size = sizeof(material_data);
	vkCmdCopyBuffer(cb, m_single_cell_sb, mat.data, 1, &region);
	end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);

	//allocate descriptor set
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_MAT];
	pool_id p_id = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT].get_available_pool_id();
	if (p_id == m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT].pools.size())
	{
		extend_descriptor_pool_pool<DESCRIPTOR_SET_LAYOUT_TYPE_MAT>();
	}
	alloc_info.descriptorPool = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT].pools[p_id];
	if (vkAllocateDescriptorSets(m_base.device, &alloc_info, &mat.ds) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate material descriptor set!");

	--m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT].availability[p_id];
	mat.pool_index = p_id;

	//update descriptor set
	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = mat.data;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(material_data);

	std::array<VkDescriptorImageInfo, TEX_TYPE_COUNT> tex_info;
	std::array<VkWriteDescriptorSet, TEX_TYPE_COUNT+1> write = {};

	write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write[0].descriptorCount = 1;
	write[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write[0].dstArrayElement = 0;
	write[0].dstBinding = 0;
	write[0].dstSet = mat.ds;
	write[0].pBufferInfo = &buffer_info;

	uint32_t write_index = 1;
	uint32_t tex_info_index = 0;
	flag = 1;
	for (uint32_t i = 0; i < TEX_TYPE_COUNT; ++i)
	{
		if (flag & data.flags)
		{
			tex_info[tex_info_index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			tex_info[tex_info_index].imageView = mat.texs[i].view;
			tex_info[tex_info_index].sampler = m_samplers[mat.texs[i].sampler_type];

			write[write_index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write[write_index].descriptorCount = 1;
			write[write_index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write[write_index].dstArrayElement = 0;
			write[write_index].dstBinding = i + 1;
			write[write_index].dstSet = mat.ds;
			write[write_index].pImageInfo = &tex_info[tex_info_index];
			
			++tex_info_index;
			++write_index;
		}
		flag = flag << 1;
	}

	vkUpdateDescriptorSets(m_base.device, write_index, write.data(), 0, nullptr);

	return mat;
}

transform resource_manager::build(const transform_data& data, USAGE usage)
{
	transform tr;
	tr.usage = usage;

	//create buffer
	VkBufferCreateInfo buffer_create_info = {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_create_info.size = sizeof(transform_data);
	buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (usage == USAGE_STATIC)
		buffer_create_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (vkCreateBuffer(m_base.device, &buffer_create_info, host_memory_manager, &tr.buffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create transform buffer!");

	//allocate memory and copy
	tr.cell = device_memory::instance()->alloc_buffer_memory(usage, tr.buffer, &tr.data);
	if (usage == USAGE_DYNAMIC)
		memcpy(tr.data, &data, sizeof(transform_data));
	else
	{
		//copy to staging buffer
		memcpy(m_single_cell_sb_data, &data, sizeof(transform_data));

		//copy to transform data buffer
		VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);

		VkBufferCopy region = {};
		region.dstOffset = 0;
		region.srcOffset = 0;
		region.size = sizeof(transform_data);
		vkCmdCopyBuffer(cb, m_single_cell_sb, tr.buffer, 1, &region);
		end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);
	}

	//allocate descriptor set
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_TR];
	pool_id p_id = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TR].get_available_pool_id();
	if (p_id == m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TR].pools.size())
	{
		extend_descriptor_pool_pool<DESCRIPTOR_SET_LAYOUT_TYPE_TR>();
	}
	alloc_info.descriptorPool = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TR].pools[p_id];
	if (vkAllocateDescriptorSets(m_base.device, &alloc_info, &tr.ds) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate transform descriptor set!");
	--m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TR].availability[p_id];
	tr.pool_index = p_id;

	//update descriptor set
	VkDescriptorBufferInfo buffer_info;
	buffer_info.buffer = tr.buffer;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(transform_data);
	
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.dstArrayElement = 0;
	write.dstBinding = 0;
	write.dstSet = tr.ds;
	write.pBufferInfo = &buffer_info;

	vkUpdateDescriptorSets(m_base.device, 1, &write, 0, nullptr);


	return tr;
}

memory resource_manager::build(const std::vector<VkMemoryAllocateInfo>& alloc_infos)
{
	memory mem(alloc_infos.size());

	for (size_t i = 0; i < mem.size(); ++i)
	{
		if (vkAllocateMemory(m_base.device, &alloc_infos[i], host_memory_manager, &mem[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate memory!");
	}

	return mem;
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
	vkFreeDescriptorSets(m_base.device, m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT].pools[_mat.pool_index], 1, &_mat.ds);
	++m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT].availability[_mat.pool_index];

	for (size_t i = 0; i < TEX_TYPE_COUNT; ++i)
	{
		if (_mat.texs[i].view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(m_base.device, _mat.texs[i].view, host_memory_manager);
			vkDestroyImage(m_base.device, _mat.texs[i].image, host_memory_manager);
			vkFreeMemory(m_base.device, _mat.texs[i].memory, host_memory_manager);
		}
	}
	
	vkDestroyBuffer(m_base.device, _mat.data, host_memory_manager);
	device_memory::instance()->free_buffer(USAGE_STATIC, _mat.cell);
}

void resource_manager::destroy(transform&& _tr)
{
	vkFreeDescriptorSets(m_base.device, m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TR].pools[_tr.pool_index], 1, &_tr.ds);
	++m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TR].availability[_tr.pool_index];

	vkDestroyBuffer(m_base.device, _tr.buffer, host_memory_manager);
	device_memory::instance()->free_buffer(_tr.usage, _tr.cell);
}

void resource_manager::destroy(light&& l)
{
	vkFreeDescriptorSets(m_base.device, m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT].pools[l.pool_index], 1, &l.ds);
	++m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TR].availability[l.pool_index];

	vkDestroyBuffer(m_base.device, l.buffer, host_memory_manager);
	device_memory::instance()->free_buffer(l.usage, l.cell);

	if (l.has_shadow_map)
	{
		vkDestroyImageView(m_base.device, l.shadow_map.view, host_memory_manager);
		vkDestroyImage(m_base.device, l.shadow_map.image, host_memory_manager);
		vkFreeMemory(m_base.device, l.shadow_map.memory, host_memory_manager);
	}
}

void resource_manager::destroy(memory && _memory)
{
	for (auto mem : _memory)
		vkFreeMemory(m_base.device, mem, host_memory_manager);
}

void rcq::resource_manager::update_tr(const std::vector<update_tr_info>& trs)
{
	size_t static_count = 0;

	for (const auto& tr_info : trs)
	{
		auto& tr = get<RESOURCE_TYPE_TR>(std::get<UPDATE_TR_INFO_TR_ID>(tr_info));
		if (tr.usage==USAGE_DYNAMIC)
			memcpy(tr.data, &std::get<UPDATE_TR_INFO_TR_DATA>(tr_info), sizeof(transform_data));
		else
			++static_count;
	}

	if (static_count!=0)
	{
		size_t cell_size = device_memory::instance()->get_cell_size(USAGE_STATIC);

		size_t i = 0;
		while (static_count != 0)
		{
			VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_update);

			VkBufferCopy region = {};
			region.size = sizeof(transform_data);
			region.dstOffset = 0;
			region.srcOffset = 0;

			size_t processed_static_count = 0;
			for (; processed_static_count < STAGING_BUFFER_CELL_COUNT && i < trs.size(); ++i)
			{
				auto tr = std::get<RESOURCE_TYPE_TR>(m_resources_ready)[std::get<UPDATE_TR_INFO_TR_ID>(trs[i])];
				if (tr.usage == USAGE_STATIC)
				{
					memcpy(m_sb_data + region.srcOffset,
						&std::get<UPDATE_TR_INFO_TR_DATA>(trs[i]), sizeof(transform_data));
					vkCmdCopyBuffer(cb, m_sb, tr.buffer, 1, &region);
					region.srcOffset += cell_size;
					++processed_static_count;
				}
			}
			static_count -= processed_static_count;

			end_single_time_command_buffer(m_base.device, m_cp_update, m_base.graphics_queue, cb);
		}
	}
}

texture rcq::resource_manager::load_texture(const std::string & filename)
{
	texture tex;
	
	//load image to staging buffer
	int width, height, channels;
	stbi_uc* pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);

	if (!pixels)
	{
		throw std::runtime_error("failed to load texture image!");
	}

	size_t image_size = width*height * 4;
	VkBuffer sb;
	VkDeviceMemory sb_mem;
	create_staging_buffer(m_base, image_size, sb, sb_mem);

	void* data;
	vkMapMemory(m_base.device, sb_mem, 0, image_size, 0, &data);
	memcpy(data, pixels, image_size);
	vkUnmapMemory(m_base.device, sb_mem);
	stbi_image_free(pixels);

	//create image
	VkImageCreateInfo image_create_info = {};
	image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_create_info.arrayLayers = 1;
	image_create_info.extent.width = static_cast<uint32_t>(width);
	image_create_info.extent.height = static_cast<uint32_t>(height);
	image_create_info.extent.depth = 1;
	image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_create_info.mipLevels = 1;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	if (vkCreateImage(m_base.device, &image_create_info, host_memory_manager, &tex.image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture image!");
	}

	//allocate memory for image
	VkMemoryRequirements mr;
	vkGetImageMemoryRequirements(m_base.device, tex.image, &mr);

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mr.size;
	alloc_info.memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(m_base.device, &alloc_info, host_memory_manager, &tex.memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate texture memory!");
	}
	vkBindImageMemory(m_base.device, tex.image, tex.memory, 0);

	//transition layout to transfer dst optimal
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.srcAccessMask = 0;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = tex.image;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);
	vkCmdPipelineBarrier(
		cb,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	//copy from staging buffer
	VkBufferImageCopy region = {};
	region.bufferImageHeight = 0;
	region.bufferRowLength = 0;
	region.bufferOffset = 0;
	region.imageExtent.depth = 1;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageOffset = { 0, 0, 0 };
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.mipLevel = 0;

	vkCmdCopyBufferToImage(cb, sb, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	//transition layout to shader read only optimal
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = tex.image;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(
		cb,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	end_single_time_command_buffer(m_base.device, m_cp_build,
		m_base.graphics_queue, cb);

	//destroy staging buffer
	vkDestroyBuffer(m_base.device, sb, host_memory_manager);
	vkFreeMemory(m_base.device, sb_mem, host_memory_manager);

	//create image view
	VkImageViewCreateInfo image_view_create_info = {};
	image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.image = tex.image;
	image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_view_create_info.subresourceRange.baseArrayLayer = 0;
	image_view_create_info.subresourceRange.baseMipLevel = 0;
	image_view_create_info.subresourceRange.layerCount = 1;
	image_view_create_info.subresourceRange.levelCount = 1;
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

	if (vkCreateImageView(m_base.device, &image_view_create_info, host_memory_manager,
		&tex.view) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture image view!");
	}

	tex.sampler_type = SAMPLER_TYPE_SIMPLE;

	return tex;
}

void rcq::resource_manager::create_samplers()
{
	VkSamplerCreateInfo simple_sampler_info = {};
	simple_sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	simple_sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	simple_sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	simple_sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	simple_sampler_info.anisotropyEnable = VK_TRUE;
	simple_sampler_info.magFilter = VK_FILTER_LINEAR;
	simple_sampler_info.minFilter = VK_FILTER_LINEAR;
	simple_sampler_info.maxAnisotropy = 16;
	simple_sampler_info.unnormalizedCoordinates = VK_FALSE;
	simple_sampler_info.compareEnable = VK_FALSE;
	simple_sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	simple_sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	simple_sampler_info.mipLodBias = 0.f;
	simple_sampler_info.minLod = 0.f;
	simple_sampler_info.maxLod = 0.f;

	if (vkCreateSampler(m_base.device, &simple_sampler_info, host_memory_manager, &m_samplers[SAMPLER_TYPE_SIMPLE]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}

	VkSamplerCreateInfo omni_light_shadow_map_sampler = {};
	omni_light_shadow_map_sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	omni_light_shadow_map_sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	omni_light_shadow_map_sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	omni_light_shadow_map_sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	omni_light_shadow_map_sampler.anisotropyEnable = VK_TRUE;
	omni_light_shadow_map_sampler.maxAnisotropy = 16.f;
	omni_light_shadow_map_sampler.compareEnable = VK_FALSE;
	omni_light_shadow_map_sampler.magFilter = VK_FILTER_LINEAR;
	omni_light_shadow_map_sampler.maxLod = 0.f;
	omni_light_shadow_map_sampler.minFilter = VK_FILTER_LINEAR;
	omni_light_shadow_map_sampler.minLod = 0.f;
	omni_light_shadow_map_sampler.mipLodBias = 0.f;
	omni_light_shadow_map_sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(m_base.device, &omni_light_shadow_map_sampler, host_memory_manager, 
		&m_samplers[SAMPLER_TYPE_OMNI_LIGHT_SHADOW_MAP]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void resource_manager::create_staging_buffers()
{
	constexpr size_t cell_size = std::max(sizeof(transform_data), sizeof(material_data));

	//create single time sb
	VkBufferCreateInfo sb_info = {};
	sb_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sb_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sb_info.size = cell_size;
	sb_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	if (vkCreateBuffer(m_base.device, &sb_info, host_memory_manager, &m_single_cell_sb) != VK_SUCCESS)
		throw std::runtime_error("failed to create staging buffer!");

	VkMemoryRequirements mr;
	vkGetBufferMemoryRequirements(m_base.device, m_single_cell_sb, &mr);
	std::vector<VkMemoryAllocateInfo> alloc_infos(2);
	alloc_infos[0].sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_infos[0].allocationSize = mr.size;
	alloc_infos[0].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	//create sb
	sb_info.size=STAGING_BUFFER_CELL_COUNT*device_memory::instance()->get_cell_size(USAGE_STATIC);
	if (vkCreateBuffer(m_base.device, &sb_info, host_memory_manager, &m_sb) != VK_SUCCESS)
		throw std::runtime_error("failed to create staging buffer!");
	vkGetBufferMemoryRequirements(m_base.device, m_sb, &mr);
	alloc_infos[1].sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_infos[1].allocationSize = mr.size;
	alloc_infos[1].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	unique_id id = ~0; //id for resource manager
	build_package build;
	std::get<RESOURCE_TYPE_MEMORY>(build).emplace_back(id, std::move(alloc_infos));
	process_build_package(std::move(build));
	memory mem = get<RESOURCE_TYPE_MEMORY>(id);
	m_single_cell_sb_mem = mem[0];
	m_sb_mem = mem[1];
	vkBindBufferMemory(m_base.device, m_single_cell_sb, m_single_cell_sb_mem, 0);
	vkBindBufferMemory(m_base.device, m_sb, m_sb_mem, 0);

	void* data;
	vkMapMemory(m_base.device, m_single_cell_sb_mem, 0, cell_size, 0, &data);
	m_single_cell_sb_data = static_cast<char*>(data);
	vkMapMemory(m_base.device, m_sb_mem, 0, cell_size*STAGING_BUFFER_CELL_COUNT, 0, &data);
	m_sb_data = static_cast<char*>(data);
}

void resource_manager::create_descriptor_set_layouts()
{
	//create transform dsl
	VkDescriptorSetLayoutBinding ub_binding = {};
	ub_binding.binding = 0;
	ub_binding.descriptorCount = 1;
	ub_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ub_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo transform_dsl_create_info = {};
	transform_dsl_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	transform_dsl_create_info.bindingCount = 1;
	transform_dsl_create_info.pBindings = &ub_binding;

	if (vkCreateDescriptorSetLayout(m_base.device, &transform_dsl_create_info, host_memory_manager, 
		&m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_TR]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create transform descriptor set layout!");
	}

	//create material dsl
	VkDescriptorSetLayoutBinding tex_binding = {};
	tex_binding.descriptorCount = 1;
	tex_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	tex_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding material_bindings[TEX_TYPE_COUNT + 1];
	material_bindings[0] = ub_binding;
	material_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	for (size_t i = 1; i < TEX_TYPE_COUNT + 1; ++i)
	{
		tex_binding.binding = i;
		material_bindings[i] = tex_binding;
	}

	VkDescriptorSetLayoutCreateInfo material_dsl_create_info = {};
	material_dsl_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	material_dsl_create_info.bindingCount = TEX_TYPE_COUNT + 1;
	material_dsl_create_info.pBindings = material_bindings;

	if (vkCreateDescriptorSetLayout(m_base.device, &material_dsl_create_info, host_memory_manager, 
		&m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_MAT]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create material descriptor set layout!");
	}

	//create light dsl
	VkDescriptorSetLayoutBinding light_bindings[2] = {};

	light_bindings[0].binding = 0;
	light_bindings[0].descriptorCount = 1;
	light_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	light_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	light_bindings[1].binding = 1;
	light_bindings[1].descriptorCount = 1;
	light_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorSetLayoutCreateInfo dsl_info = {};
	dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsl_info.bindingCount = 2;
	dsl_info.pBindings = light_bindings;

	if (vkCreateDescriptorSetLayout(m_base.device, &dsl_info, host_memory_manager, 
		&m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT]) != VK_SUCCESS)
		throw std::runtime_error("failed to create light descriptor set layout!");
}

template<DESCRIPTOR_SET_LAYOUT_TYPE dsl_type>
inline void resource_manager::extend_descriptor_pool_pool()
{
	VkDescriptorPoolCreateInfo dp_info = {};
	dp_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dp_info.maxSets = DESCRIPTOR_POOL_SIZE;
	dp_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	std::array<VkDescriptorPoolSize, 2> dp_size = {};
	dp_info.pPoolSizes = dp_size.data();

	if constexpr (dsl_type == DESCRIPTOR_SET_LAYOUT_TYPE_TR)
	{
		dp_size[0].descriptorCount = DESCRIPTOR_POOL_SIZE;
		dp_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		dp_info.poolSizeCount = 1;
	}

	if constexpr (dsl_type == DESCRIPTOR_SET_LAYOUT_TYPE_MAT)
	{
		
		dp_size[0].descriptorCount = DESCRIPTOR_POOL_SIZE;
		dp_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		dp_size[1].descriptorCount = TEX_TYPE_COUNT*DESCRIPTOR_POOL_SIZE;
		dp_size[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		dp_info.poolSizeCount = 2;

	}

	if constexpr (dsl_type == DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT)
	{
		dp_size[0].descriptorCount = DESCRIPTOR_POOL_SIZE;
		dp_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		dp_size[1].descriptorCount = DESCRIPTOR_POOL_SIZE;
		dp_size[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		dp_info.poolSizeCount = 2;
	}

	VkDescriptorPool new_pool;
	if (vkCreateDescriptorPool(m_base.device, &dp_info, host_memory_manager, &new_pool) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor pool!");

	m_dpps[dsl_type].pools.push_back(new_pool);
	m_dpps[dsl_type].availability.push_back(DESCRIPTOR_POOL_SIZE);
}

void resource_manager::create_command_pool()
{
	VkCommandPoolCreateInfo cp_info = {};
	cp_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cp_info.queueFamilyIndex = m_base.queue_families.graphics_family;
	cp_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	if (vkCreateCommandPool(m_base.device, &cp_info, host_memory_manager, &m_cp_build) != VK_SUCCESS ||
		vkCreateCommandPool(m_base.device, &cp_info, host_memory_manager, &m_cp_update) != VK_SUCCESS)
		throw std::runtime_error("failed to create command pool!");
}

light resource_manager::build(const light_data& data, USAGE usage, bool make_shadow_map)
{
	light l;
	l.has_shadow_map = make_shadow_map;
	l.usage = usage;
	l.type = static_cast<LIGHT_TYPE>(data.index());

	//create buffer
	VkBufferCreateInfo buffer_create_info = {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_create_info.size = sizeof(light_data);
	buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (usage == USAGE_STATIC)
		buffer_create_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (vkCreateBuffer(m_base.device, &buffer_create_info, host_memory_manager, &l.buffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create transform buffer!");

	//allocate memory and copy
	l.cell = device_memory::instance()->alloc_buffer_memory(usage, l.buffer, &l.data);
	if (usage == USAGE_DYNAMIC)
		memcpy(l.data, &data, sizeof(light_data));
	else
	{
		//copy to staging buffer
		memcpy(m_single_cell_sb_data, &data, sizeof(light_data));

		//copy to transform data buffer
		VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);

		VkBufferCopy region = {};
		region.dstOffset = 0;
		region.srcOffset = 0;
		region.size = sizeof(light_data);
		vkCmdCopyBuffer(cb, m_single_cell_sb, l.buffer, 1, &region);
		end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);
	}

	//allocate descriptor set
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT];
	pool_id p_id = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT].get_available_pool_id();
	if (p_id == m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT].pools.size())
	{
		extend_descriptor_pool_pool<DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT>();
	}
	alloc_info.descriptorPool = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT].pools[p_id];
	if (vkAllocateDescriptorSets(m_base.device, &alloc_info, &l.ds) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate transform descriptor set!");
	--m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT].availability[p_id];
	l.pool_index = p_id;

	//create shadow map (currently for omi light only)
	if (make_shadow_map)
	{
		if (data.index() == LIGHT_TYPE_OMNI)
		{
			//create image
			VkImageCreateInfo image_info = {};
			image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			image_info.arrayLayers = 6;
			image_info.extent.depth = 1;
			image_info.extent.height = SHADOW_MAP_SIZE;
			image_info.extent.width = SHADOW_MAP_SIZE;
			image_info.format = VK_FORMAT_D32_SFLOAT;
			image_info.imageType = VK_IMAGE_TYPE_2D;
			image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image_info.mipLevels = 1;
			image_info.samples = VK_SAMPLE_COUNT_1_BIT;
			image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
			image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			if (vkCreateImage(m_base.device, &image_info, host_memory_manager, &l.shadow_map.image) != VK_SUCCESS)
				throw std::runtime_error("failed to create image!");

			//alloc memory
			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, l.shadow_map.image, &mr);

			VkMemoryAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.allocationSize = mr.size;
			alloc_info.memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			
			if (vkAllocateMemory(m_base.device, &alloc_info, host_memory_manager, &l.shadow_map.memory) != VK_SUCCESS)
				throw std::runtime_error("failed to allocate memory!");
			vkBindImageMemory(m_base.device, l.shadow_map.image, l.shadow_map.memory, 0);

			//create image view
			VkImageViewCreateInfo view_info = {};
			view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view_info.format = VK_FORMAT_D32_SFLOAT;
			view_info.image = l.shadow_map.image;
			view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			view_info.subresourceRange.baseArrayLayer = 0;
			view_info.subresourceRange.baseMipLevel = 0;
			view_info.subresourceRange.layerCount = 6;
			view_info.subresourceRange.levelCount = 1;

			if (vkCreateImageView(m_base.device, &view_info, host_memory_manager, &l.shadow_map.view) != VK_SUCCESS)
				throw std::runtime_error("failed to create image view!");

			//transition to depth stencil optimal layout
			VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);

			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			barrier.image = l.shadow_map.image;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			barrier.srcAccessMask = 0;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.layerCount = 6;
			barrier.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr,
				0, nullptr, 1, &barrier);

			end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);

			l.shadow_map.sampler_type = SAMPLER_TYPE_OMNI_LIGHT_SHADOW_MAP;
		}
	}

	//update descriptor set
	VkDescriptorBufferInfo buffer_info;
	buffer_info.buffer = l.buffer;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(light_data);

	VkWriteDescriptorSet write[2] = {};
	write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write[0].descriptorCount = 1;
	write[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write[0].dstArrayElement = 0;
	write[0].dstBinding = 0;
	write[0].dstSet = l.ds;
	write[0].pBufferInfo = &buffer_info;

	if (make_shadow_map)
	{
		VkDescriptorImageInfo shadow_map_info = {};
		shadow_map_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadow_map_info.imageView = l.shadow_map.view;
		shadow_map_info.sampler = m_samplers[SAMPLER_TYPE_OMNI_LIGHT_SHADOW_MAP];

		write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[1].descriptorCount = 1;
		write[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[1].dstArrayElement = 0;
		write[1].dstBinding = 1;
		write[1].dstSet = l.ds;
		write[1].pImageInfo = &shadow_map_info;

		vkUpdateDescriptorSets(m_base.device, 2, write, 0, nullptr);
	}
	else
	{
		vkUpdateDescriptorSets(m_base.device, 1, write, 0, nullptr);
	}

	return l;
}