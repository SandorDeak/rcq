#include "resource_manager.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "device_memory.h"
#include "gta5_pass.h"
//#include "omni_light_shadow_pass.h"
//#include "basic_pass.h"

using namespace rcq;

resource_manager* resource_manager::m_instance = nullptr;

void load_mesh(const std::string& file_name, std::vector<vertex>& vertices, std::vector<uint32_t>& indices,
	bool make_vertex_ext, std::vector<vertex_ext>& vertices_ext);

resource_manager::resource_manager(const base_info& info) : m_base(info), m_alloc("resource_manager")
{
	m_current_build_task_p.reset(new build_task_package);

	create_samplers();
	create_descriptor_set_layouts();
	create_command_pool();

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

	create_staging_buffers();

}


resource_manager::~resource_manager()
{
	m_should_end_build = true;
	m_build_thread.join();

	vkUnmapMemory(m_base.device, m_single_cell_sb_mem);
	vkUnmapMemory(m_base.device, m_sb_mem);
	vkDestroyBuffer(m_base.device, m_single_cell_sb, m_alloc);
	vkDestroyBuffer(m_base.device, m_sb, m_alloc);
	auto destroy = std::make_unique<destroy_package>();
	destroy->ids[RESOURCE_TYPE_MEMORY].push_back(~0);
	push_destroy_package(std::move(destroy));
	
	m_should_end_destroy = true;
	m_destroy_thread.join();	

	for (auto& sampler : m_samplers)
		vkDestroySampler(m_base.device, sampler, m_alloc);
	for (auto& dsl : m_dsls)
		vkDestroyDescriptorSetLayout(m_base.device, dsl, m_alloc);
	for (auto& dpp : m_dpps)
	{
		for (auto& dp : dpp.pools)
			vkDestroyDescriptorPool(m_base.device, dp, m_alloc);
	}
	vkDestroyCommandPool(m_base.device, m_cp_build, m_alloc);
	vkDestroyCommandPool(m_base.device, m_cp_update, m_alloc);

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

	create_build_tasks(std::move(package), std::make_index_sequence<RESOURCE_TYPE_COUNT>());

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
			{
				package->destroy_confirmation.value().wait();
				gta5_pass::instance()->wait_for_finish();
				//wait_for_finish(std::make_index_sequence<RENDER_PASS_COUNT>());
			}
			/*bool has_user_resource = false;
			for (int i = 0; i < RESOURCE_TYPE_COUNT - 1; ++i)
			{
				if (!package->ids[i].empty())
					has_user_resource = true;
			}
			if (has_user_resource)
				wait_for_finish(std::make_index_sequence<RENDER_PASS_COUNT>());*/
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

template<>
mesh resource_manager::build<RESOURCE_TYPE_MESH>(const std::string& filename, bool calc_tb)
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

	if (vkCreateBuffer(m_base.device, &vb_info, m_alloc, &_mesh.vb) != VK_SUCCESS)
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

	if (vkCreateBuffer(m_base.device, &ib_info, m_alloc, &_mesh.ib) != VK_SUCCESS)
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

		if (vkCreateBuffer(m_base.device, &veb_info, m_alloc, &_mesh.veb) != VK_SUCCESS)
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

	if (vkAllocateMemory(m_base.device, &alloc_info, m_alloc, &_mesh.memory) != VK_SUCCESS)
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
	create_staging_buffer(m_base, sb_size, sb, sb_mem, m_alloc);

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
	vkDestroyBuffer(m_base.device, sb, m_alloc);
	vkFreeMemory(m_base.device, sb_mem, m_alloc);

	return _mesh;
}
template<>
material_opaque resource_manager::build<RESOURCE_TYPE_MAT_OPAQUE>(const material_opaque_data& data, const texfiles& files)
{
	material_opaque mat;

	//load textures	
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
	buffer_create_info.size = sizeof(material_opaque_data);
	buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (vkCreateBuffer(m_base.device, &buffer_create_info, m_alloc, &mat.data) != VK_SUCCESS)
		throw std::runtime_error("failed to create material data buffer!");

	//allocate memory for material data buffer
	mat.cell = device_memory::instance()->alloc_buffer_memory(USAGE_STATIC, mat.data, nullptr);

	//copy to staging buffer
	memcpy(m_single_cell_sb_data, &data, sizeof(material_opaque_data));

	//copy to material data buffer
	VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);

	VkBufferCopy region = {};
	region.dstOffset = 0;
	region.srcOffset = 0;
	region.size = sizeof(material_opaque_data);
	vkCmdCopyBuffer(cb, m_single_cell_sb, mat.data, 1, &region);
	end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);

	//allocate descriptor set
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE];
	pool_id p_id = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE].get_available_pool_id();
	if (p_id == m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE].pools.size())
	{
		extend_descriptor_pool_pool<DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE>();
	}
	alloc_info.descriptorPool = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE].pools[p_id];
	if (vkAllocateDescriptorSets(m_base.device, &alloc_info, &mat.ds) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate material descriptor set!");

	--m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE].availability[p_id];
	mat.pool_index = p_id;

	//update descriptor set
	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = mat.data;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(material_opaque_data);

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

template<>
transform resource_manager::build<RESOURCE_TYPE_TR>(const transform_data& data, USAGE usage)
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
	if (vkCreateBuffer(m_base.device, &buffer_create_info, m_alloc, &tr.buffer) != VK_SUCCESS)
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

template<>
memory resource_manager::build<RESOURCE_TYPE_MEMORY>(const std::vector<VkMemoryAllocateInfo>& alloc_infos)
{
	memory mem(alloc_infos.size());

	for (size_t i = 0; i < mem.size(); ++i)
	{
		if (vkAllocateMemory(m_base.device, &alloc_infos[i], m_alloc, &mem[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate memory!");
	}

	return mem;
}

void resource_manager::destroy(mesh&& _mesh)
{
	vkDestroyBuffer(m_base.device, _mesh.ib, m_alloc);
	vkDestroyBuffer(m_base.device, _mesh.vb, m_alloc);
	if (_mesh.veb != VK_NULL_HANDLE)
		vkDestroyBuffer(m_base.device, _mesh.veb, m_alloc);

	vkFreeMemory(m_base.device, _mesh.memory, m_alloc);
}

void resource_manager::destroy(material_opaque&& _mat)
{
	vkFreeDescriptorSets(m_base.device, m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE].pools[_mat.pool_index], 1, &_mat.ds);
	++m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE].availability[_mat.pool_index];

	for (size_t i = 0; i < TEX_TYPE_COUNT; ++i)
	{
		if (_mat.texs[i].view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(m_base.device, _mat.texs[i].view, m_alloc);
			vkDestroyImage(m_base.device, _mat.texs[i].image, m_alloc);
			vkFreeMemory(m_base.device, _mat.texs[i].memory, m_alloc);
		}
	}
	
	vkDestroyBuffer(m_base.device, _mat.data, m_alloc);
	device_memory::instance()->free_buffer(USAGE_STATIC, _mat.cell);
}

void resource_manager::destroy(transform&& _tr)
{
	vkFreeDescriptorSets(m_base.device, m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TR].pools[_tr.pool_index], 1, &_tr.ds);
	++m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TR].availability[_tr.pool_index];

	vkDestroyBuffer(m_base.device, _tr.buffer, m_alloc);
	device_memory::instance()->free_buffer(_tr.usage, _tr.cell);
}

void resource_manager::destroy(light_omni&& l)
{
	vkFreeDescriptorSets(m_base.device, m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI].pools[l.pool_index], 1, &l.ds);
	++m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TR].availability[l.pool_index];

	vkDestroyBuffer(m_base.device, l.buffer, m_alloc);
	device_memory::instance()->free_buffer(l.usage, l.cell);

	if (l.shadow_map.image!=VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(m_base.device, l.shadow_map_fb, m_alloc);
		vkDestroyImageView(m_base.device, l.shadow_map.view, m_alloc);
		vkDestroyImage(m_base.device, l.shadow_map.image, m_alloc);
		vkFreeMemory(m_base.device, l.shadow_map.memory, m_alloc);
	}
}

void resource_manager::destroy(memory && _memory)
{
	for (auto mem : _memory)
		vkFreeMemory(m_base.device, mem, m_alloc);
}

void rcq::resource_manager::update_tr(const std::vector<update_tr_info>& trs)
{
	size_t static_count = 0;

	for (const auto& tr_info : trs)
	{
		auto& tr = get_res<RESOURCE_TYPE_TR>(std::get<UPDATE_TR_INFO_TR_ID>(tr_info));
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
	create_staging_buffer(m_base, image_size, sb, sb_mem, m_alloc);

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

	if (vkCreateImage(m_base.device, &image_create_info, m_alloc, &tex.image) != VK_SUCCESS)
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

	if (vkAllocateMemory(m_base.device, &alloc_info, m_alloc, &tex.memory) != VK_SUCCESS)
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
	vkDestroyBuffer(m_base.device, sb, m_alloc);
	vkFreeMemory(m_base.device, sb_mem, m_alloc);

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

	if (vkCreateImageView(m_base.device, &image_view_create_info, m_alloc,
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

	if (vkCreateSampler(m_base.device, &simple_sampler_info, m_alloc, &m_samplers[SAMPLER_TYPE_SIMPLE]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}

	VkSamplerCreateInfo omni_light_shadow_map_sampler = {};
	omni_light_shadow_map_sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	omni_light_shadow_map_sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	omni_light_shadow_map_sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	omni_light_shadow_map_sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	omni_light_shadow_map_sampler.anisotropyEnable = VK_FALSE;
	omni_light_shadow_map_sampler.maxAnisotropy = 16.f;
	omni_light_shadow_map_sampler.compareEnable = VK_FALSE;
	omni_light_shadow_map_sampler.magFilter = VK_FILTER_LINEAR;
	omni_light_shadow_map_sampler.maxLod = 0.f;
	omni_light_shadow_map_sampler.minFilter = VK_FILTER_LINEAR;
	omni_light_shadow_map_sampler.minLod = 0.f;
	omni_light_shadow_map_sampler.mipLodBias = 0.f;
	omni_light_shadow_map_sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(m_base.device, &omni_light_shadow_map_sampler, m_alloc, 
		&m_samplers[SAMPLER_TYPE_CUBE]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void resource_manager::create_staging_buffers()
{
	constexpr size_t cell_size = std::max(sizeof(transform_data), sizeof(material_opaque_data));

	//create single time sb
	VkBufferCreateInfo sb_info = {};
	sb_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sb_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sb_info.size = cell_size;
	sb_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	if (vkCreateBuffer(m_base.device, &sb_info, m_alloc, &m_single_cell_sb) != VK_SUCCESS)
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
	if (vkCreateBuffer(m_base.device, &sb_info, m_alloc, &m_sb) != VK_SUCCESS)
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
	memory mem = get_res<RESOURCE_TYPE_MEMORY>(id);
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

	if (vkCreateDescriptorSetLayout(m_base.device, &transform_dsl_create_info, m_alloc, 
		&m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_TR]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create transform descriptor set layout!");
	}

	//create material opaque dsl
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

	if (vkCreateDescriptorSetLayout(m_base.device, &material_dsl_create_info, m_alloc, 
		&m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create material descriptor set layout!");
	}

	//create material em
	{
		VkDescriptorSetLayoutBinding tex_binding = {};
		tex_binding.binding = 0;
		tex_binding.descriptorCount = 1;
		tex_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		tex_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = &tex_binding;

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, m_alloc, 
			&m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_MAT_EM]) != VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//create sky dsl
	{
		std::array<VkDescriptorSetLayoutBinding, 3> bindings  = {};
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[2].binding = 2;
		bindings[2].descriptorCount = 1;
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = bindings.size();
		dsl.pBindings = bindings.data();
		
		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, m_alloc,
			&m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_SKY]) != VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//create terrain dsl
	{
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorCount=1;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = &binding;

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, m_alloc,
			&m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN]) != VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//create light omni dsl
	VkDescriptorSetLayoutBinding light_bindings[2] = {};

	light_bindings[0].binding = 0;
	light_bindings[0].descriptorCount = 1;
	light_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	light_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_VERTEX_BIT;

	light_bindings[1].binding = 1;
	light_bindings[1].descriptorCount = 1;
	light_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	light_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo dsl_info = {};
	dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsl_info.bindingCount = 2;
	dsl_info.pBindings = light_bindings;

	if (vkCreateDescriptorSetLayout(m_base.device, &dsl_info, m_alloc, 
		&m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI]) != VK_SUCCESS)
		throw std::runtime_error("failed to create light descriptor set layout!");

	//create skybox dsl
	VkDescriptorSetLayoutBinding cubemap_binding = {};
	cubemap_binding.binding = 0;
	cubemap_binding.descriptorCount = 1;
	cubemap_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	cubemap_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo skybox_dsl = {};
	skybox_dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	skybox_dsl.bindingCount = 1;
	skybox_dsl.pBindings = &cubemap_binding;

	if (vkCreateDescriptorSetLayout(m_base.device, &skybox_dsl, m_alloc,
		&m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX]) != VK_SUCCESS)
		throw std::runtime_error("failde to create skybox descriptor set layout!");
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

	if constexpr (dsl_type == DESCRIPTOR_SET_LAYOUT_TYPE_SKY)
	{
		dp_size[0].descriptorCount = DESCRIPTOR_POOL_SIZE*3;
		dp_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		dp_info.poolSizeCount = 1;
	}

	if constexpr (dsl_type == DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN)
	{
		dp_size[0].descriptorCount = DESCRIPTOR_POOL_SIZE;
		dp_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		dp_info.poolSizeCount = 1;
	}

	if constexpr (dsl_type == DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE)
	{
		
		dp_size[0].descriptorCount = DESCRIPTOR_POOL_SIZE;
		dp_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		dp_size[1].descriptorCount = TEX_TYPE_COUNT*DESCRIPTOR_POOL_SIZE;
		dp_size[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		dp_info.poolSizeCount = 2;

	}

	if constexpr (dsl_type == DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI)
	{
		dp_size[0].descriptorCount = DESCRIPTOR_POOL_SIZE;
		dp_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		dp_size[1].descriptorCount = DESCRIPTOR_POOL_SIZE;
		dp_size[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		dp_info.poolSizeCount = 2;
	}

	if constexpr (dsl_type == DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX)
	{
		dp_size[0].descriptorCount = DESCRIPTOR_POOL_SIZE;
		dp_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		dp_info.poolSizeCount = 1;
	}

	VkDescriptorPool new_pool;
	if (vkCreateDescriptorPool(m_base.device, &dp_info, m_alloc, &new_pool) != VK_SUCCESS)
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

	if (vkCreateCommandPool(m_base.device, &cp_info, m_alloc, &m_cp_build) != VK_SUCCESS ||
		vkCreateCommandPool(m_base.device, &cp_info, m_alloc, &m_cp_update) != VK_SUCCESS)
		throw std::runtime_error("failed to create command pool!");
}

template<>
light_omni resource_manager::build<RESOURCE_TYPE_LIGHT_OMNI>(const light_omni_data& data, USAGE usage)
{
	light_omni l;
	l.usage = usage;

	//create buffer
	VkBufferCreateInfo buffer_create_info = {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_create_info.size = sizeof(light_omni_data);
	buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (usage == USAGE_STATIC)
		buffer_create_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (vkCreateBuffer(m_base.device, &buffer_create_info, m_alloc, &l.buffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create transform buffer!");

	//allocate memory and copy
	l.cell = device_memory::instance()->alloc_buffer_memory(usage, l.buffer, &l.data);
	if (usage == USAGE_DYNAMIC)
		memcpy(l.data, &data, sizeof(light_omni_data));
	else
	{
		//copy to staging buffer
		memcpy(m_single_cell_sb_data, &data, sizeof(light_omni_data));

		//copy to transform data buffer
		VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);

		VkBufferCopy region = {};
		region.dstOffset = 0;
		region.srcOffset = 0;
		region.size = sizeof(light_omni_data);
		vkCmdCopyBuffer(cb, m_single_cell_sb, l.buffer, 1, &region);
		end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);
	}

	//allocate descriptor set
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI];
	pool_id p_id = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI].get_available_pool_id();
	if (p_id == m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI].pools.size())
	{
		extend_descriptor_pool_pool<DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI>();
	}
	alloc_info.descriptorPool = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI].pools[p_id];
	if (vkAllocateDescriptorSets(m_base.device, &alloc_info, &l.ds) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate transform descriptor set!");
	--m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI].availability[p_id];
	l.pool_index = p_id;

	//create shadow map (currently for omi light only)
	if (data.flags & LIGHT_FLAG_SHADOW_MAP)
	{
		//create image
		VkImageCreateInfo image_info = {};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		image_info.arrayLayers = 6;
		image_info.extent.depth = 1;
		image_info.extent.height = OMNI_SHADOW_MAP_SIZE;
		image_info.extent.width = OMNI_SHADOW_MAP_SIZE;
		image_info.format = VK_FORMAT_D32_SFLOAT;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_info.mipLevels = 1;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		if (vkCreateImage(m_base.device, &image_info, m_alloc, &l.shadow_map.image) != VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		//alloc memory
		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, l.shadow_map.image, &mr);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mr.size;
		alloc_info.memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(m_base.device, &alloc_info, m_alloc, &l.shadow_map.memory) != VK_SUCCESS)
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

		if (vkCreateImageView(m_base.device, &view_info, m_alloc, &l.shadow_map.view) != VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");

		//transition to shader read only optimal
		VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.image = l.shadow_map.image;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = 0;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.layerCount = 6;
		barrier.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
			0, nullptr, 1, &barrier);

		end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);

		l.shadow_map.sampler_type = SAMPLER_TYPE_CUBE;

		//omni_light_shadow_pass::instance()->create_framebuffer(l);

	}
	else
	{
		l.shadow_map.image = VK_NULL_HANDLE;
	}

	//update descriptor set
	VkDescriptorBufferInfo buffer_info;
	buffer_info.buffer = l.buffer;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(light_omni_data);

	VkWriteDescriptorSet write[2] = {};
	write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write[0].descriptorCount = 1;
	write[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write[0].dstArrayElement = 0;
	write[0].dstBinding = 0;
	write[0].dstSet = l.ds;
	write[0].pBufferInfo = &buffer_info;

	if (data.flags & LIGHT_FLAG_SHADOW_MAP)
	{
		VkDescriptorImageInfo shadow_map_info = {};
		shadow_map_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadow_map_info.imageView = l.shadow_map.view;
		shadow_map_info.sampler = m_samplers[SAMPLER_TYPE_CUBE];

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

template<size_t... render_passes>
void resource_manager::wait_for_finish(std::index_sequence<render_passes...>)
{
	auto l = { (render_pass_typename<render_passes>::type::instance()->wait_for_finish(), 0)... };
}

void resource_manager::destroy(skybox&& sb)
{
	vkFreeDescriptorSets(m_base.device, m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX].pools[sb.pool_index], 1, &sb.ds);
	++m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX].availability[sb.pool_index];

	vkDestroyImageView(m_base.device, sb.tex.view, m_alloc);
	vkDestroyImage(m_base.device, sb.tex.image, m_alloc);
	vkFreeMemory(m_base.device, sb.tex.memory, m_alloc);
}

template<>
skybox resource_manager::build<RESOURCE_TYPE_SKYBOX>(const std::string & filename)
{
	static const std::string postfix[6] = { "/posx.jpg", "/negx.jpg", "/posy.jpg", "/negy.jpg", "/negz.jpg", "/posz.jpg" };

	skybox skyb;

	std::array<stbi_uc*, 6> pictures;

	int width, height, channels;

	for (uint32_t i=0; i<6; ++i)
	{
		pictures[i] = stbi_load((filename + postfix[i]).c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (!pictures[i])
			throw std::runtime_error("failed to load skybox image!");
	}


	size_t image_size = width*height * 4;

	VkBuffer sb;
	VkDeviceMemory sb_mem;
	create_staging_buffer(m_base, 6 * image_size, sb, sb_mem, m_alloc);

	void* raw_data;
	vkMapMemory(m_base.device, sb_mem, 0, 6 * image_size, 0, &raw_data);
	char* data = static_cast<char*>(raw_data);

	for (int i = 0; i < 6; ++i)
	{
		memcpy(data, pictures[i], image_size);
		stbi_image_free(pictures[i]);
		data += image_size;
	}	
	vkUnmapMemory(m_base.device, sb_mem);

	//create image
	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.arrayLayers = 6;
	image_info.extent.depth = 1;
	image_info.extent.width = static_cast<uint32_t>(width);
	image_info.extent.height = static_cast<uint32_t>(height);
	image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.mipLevels = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	if (vkCreateImage(m_base.device, &image_info, m_alloc, &skyb.tex.image) != VK_SUCCESS)
		throw std::runtime_error("failed to create skybox image!");

	//allocate memory
	VkMemoryRequirements mr;
	vkGetImageMemoryRequirements(m_base.device, skyb.tex.image, &mr);

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mr.size;
	alloc_info.memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	if (vkAllocateMemory(m_base.device, &alloc_info, m_alloc, &skyb.tex.memory) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate memory for skybox!");

	vkBindImageMemory(m_base.device, skyb.tex.image, skyb.tex.memory, 0);

	VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);
	//transition image to transfer dst optimal
	VkImageMemoryBarrier barrier0 = {};
	barrier0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier0.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier0.image = skyb.tex.image;
	barrier0.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier0.srcAccessMask = 0;
	barrier0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier0.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier0.subresourceRange.baseArrayLayer = 0;
	barrier0.subresourceRange.baseMipLevel = 0;
	barrier0.subresourceRange.layerCount = 6;
	barrier0.subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
		0, nullptr, 1, &barrier0);

	//copy from staging buffer
	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferImageHeight = 0;
	region.bufferRowLength = 0;
	region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	region.imageOffset = { 0,0,0 };
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 6;
	region.imageSubresource.mipLevel = 0;

	vkCmdCopyBufferToImage(cb, sb, skyb.tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	//transition to shader read only optimal layout
	VkImageMemoryBarrier barrier1 = {};
	barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier1.image = skyb.tex.image;
	barrier1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier1.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier1.subresourceRange.baseArrayLayer = 0;
	barrier1.subresourceRange.baseMipLevel = 0;
	barrier1.subresourceRange.layerCount = 6;
	barrier1.subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
		0, nullptr, 1, &barrier1);

	end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);	

	//destroy staging buffer
	vkDestroyBuffer(m_base.device, sb, m_alloc);
	vkFreeMemory(m_base.device, sb_mem, m_alloc);

	//create image view
	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	view_info.image = skyb.tex.image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.layerCount = 6;
	view_info.subresourceRange.levelCount = 1;

	if (vkCreateImageView(m_base.device, &view_info, m_alloc, &skyb.tex.view) != VK_SUCCESS)
		throw std::runtime_error("failed to create skybox image view!");

	skyb.tex.sampler_type = SAMPLER_TYPE_CUBE;

	//allocate ds
	VkDescriptorSetAllocateInfo ds_alloc = {};
	ds_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	skyb.pool_index = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX].get_available_pool_id();
	if (skyb.pool_index == m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX].pools.size())
		extend_descriptor_pool_pool<DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX>();
	--m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX].availability[skyb.pool_index];
	ds_alloc.descriptorPool = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX].pools[skyb.pool_index];
	ds_alloc.descriptorSetCount = 1;
	ds_alloc.pSetLayouts = &m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX];
	
	if (vkAllocateDescriptorSets(m_base.device, &ds_alloc, &skyb.ds) != VK_SUCCESS)
		throw std::runtime_error("failde to allocate skybox descriptor set!");

	//update ds
	VkDescriptorImageInfo update_image = {};
	update_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	update_image.imageView = skyb.tex.view;
	update_image.sampler = m_samplers[skyb.tex.sampler_type];

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.dstArrayElement = 0;
	write.dstBinding = 0;
	write.dstSet = skyb.ds;
	write.pImageInfo = &update_image;

	vkUpdateDescriptorSets(m_base.device, 1, &write, 0, nullptr);

	return skyb;
}

template<>
sky resource_manager::build<RESOURCE_TYPE_SKY>(const std::string& filename, const glm::uvec3& sky_image_size, 
	const glm::uvec2& transmittance_size)
{
	static const std::array<std::string, 3> postfix = { "_Rayleigh.sky", "_Mie.sky", "_transmittance.sky" };

	sky s;

	VkBuffer sb;
	VkDeviceMemory sb_mem;
	VkDeviceSize sb_size = sizeof(glm::vec4)*std::max(sky_image_size.x*sky_image_size.y*sky_image_size.z,
		transmittance_size.x*transmittance_size.y);
	create_staging_buffer(m_base, sb_size, sb, sb_mem, m_alloc);
	for (uint32_t i = 0; i < 2; ++i)
	{

		//load image from file to staging buffer;
		auto LUT = read_file(filename + postfix[i]);
		void* data;
		vkMapMemory(m_base.device, sb_mem, 0, sb_size, 0, &data);
		memcpy(data, LUT.data(), LUT.size());
		vkUnmapMemory(m_base.device, sb_mem);
		LUT.clear();

		//create texture
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.extent = { sky_image_size.x, sky_image_size.y, sky_image_size.z };
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.arrayLayers = 1;
			image.imageType = VK_IMAGE_TYPE_3D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			if (vkCreateImage(m_base.device, &image, m_alloc, &s.tex[i].image) != VK_SUCCESS)
				throw std::runtime_error("failed to create sky image!");

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, s.tex[i].image, &mr);
			VkMemoryAllocateInfo alloc = {};
			alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc.memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			alloc.allocationSize = mr.size;
			if (vkAllocateMemory(m_base.device, &alloc, m_alloc, &s.tex[i].memory) != VK_SUCCESS)
				throw std::runtime_error("failed to allocate memory!");

			vkBindImageMemory(m_base.device, s.tex[i].image, s.tex[i].memory, 0);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = s.tex[i].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_3D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			if (vkCreateImageView(m_base.device, &view, m_alloc, &s.tex[i].view) != VK_SUCCESS)
				throw std::runtime_error("failed to reate sky image view!");

			s.tex[i].sampler_type = SAMPLER_TYPE_CUBE;
		}

		//copy and transition layout
		{
		VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);

		//transition to transfer dst optimal
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = s.tex[i].image;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = 0;
			b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
				0, nullptr, 1, &b);
		}

		//copy from staging buffer
		{
			VkBufferImageCopy region = {};
			region.bufferImageHeight = 0;
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.imageExtent = { sky_image_size.x, sky_image_size.y, sky_image_size.z };
			region.imageOffset = { 0,0,0 };
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.mipLevel = 0;

			vkCmdCopyBufferToImage(cb, sb, s.tex[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		//transition to shader read only optimal
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = s.tex[i].image;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
				0, nullptr, 1, &b);
		}

		end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);
		}
	}

	// load transmittance texture
	{
		//load image from file to staging buffer;
		auto LUT = read_file(filename + postfix[2]);
		void* data;
		vkMapMemory(m_base.device, sb_mem, 0, LUT.size(), 0, &data);
		memcpy(data, LUT.data(), LUT.size());
		vkUnmapMemory(m_base.device, sb_mem);
		LUT.clear();

		//create texture
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.extent = { transmittance_size.x, transmittance_size.y, 1 };
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.arrayLayers = 1;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			if (vkCreateImage(m_base.device, &image, m_alloc, &s.tex[2].image) != VK_SUCCESS)
				throw std::runtime_error("failed to create sky image!");

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, s.tex[2].image, &mr);
			VkMemoryAllocateInfo alloc = {};
			alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc.memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			alloc.allocationSize = mr.size;
			if (vkAllocateMemory(m_base.device, &alloc, m_alloc, &s.tex[2].memory) != VK_SUCCESS)
				throw std::runtime_error("failed to allocate memory!");

			vkBindImageMemory(m_base.device, s.tex[2].image, s.tex[2].memory, 0);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = s.tex[2].image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			if (vkCreateImageView(m_base.device, &view, m_alloc, &s.tex[2].view) != VK_SUCCESS)
				throw std::runtime_error("failed to reate sky image view!");

			s.tex[2].sampler_type = SAMPLER_TYPE_CUBE;
		}

		//copy and transition layout
		{
			VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);

			//transition to transfer dst optimal
			{
				VkImageMemoryBarrier b = {};
				b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				b.image = s.tex[2].image;
				b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.srcAccessMask = 0;
				b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				b.subresourceRange.baseArrayLayer = 0;
				b.subresourceRange.baseMipLevel = 0;
				b.subresourceRange.layerCount = 1;
				b.subresourceRange.levelCount = 1;

				vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
					0, nullptr, 1, &b);
			}

			//copy from staging buffer
			{
				VkBufferImageCopy region = {};
				region.bufferImageHeight = 0;
				region.bufferOffset = 0;
				region.bufferRowLength = 0;
				region.imageExtent = { transmittance_size.x, transmittance_size.y, 1 };
				region.imageOffset = { 0,0,0 };
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageSubresource.mipLevel = 0;

				vkCmdCopyBufferToImage(cb, sb, s.tex[2].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			}

			//transition to shader read only optimal
			{
				VkImageMemoryBarrier b = {};
				b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				b.image = s.tex[2].image;
				b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				b.subresourceRange.baseArrayLayer = 0;
				b.subresourceRange.baseMipLevel = 0;
				b.subresourceRange.layerCount = 1;
				b.subresourceRange.levelCount = 1;

				vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
					0, nullptr, 1, &b);
			}

			end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);
		}
	}
	//destroy staging buffer
	vkDestroyBuffer(m_base.device, sb, m_alloc);
	vkFreeMemory(m_base.device, sb_mem, m_alloc);

	//allocate descriptor set
	{
		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_SKY];
		pool_id p_id = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKY].get_available_pool_id();
		if (p_id == m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKY].pools.size())
		{
			extend_descriptor_pool_pool<DESCRIPTOR_SET_LAYOUT_TYPE_SKY>();
		}
		alloc_info.descriptorPool = m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKY].pools[p_id];
		if (vkAllocateDescriptorSets(m_base.device, &alloc_info, &s.ds) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate material descriptor set!");

		--m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKY].availability[p_id];
		s.pool_index = p_id;
	}

	//update ds
	{
		VkDescriptorImageInfo Rayleigh_tex;
		Rayleigh_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Rayleigh_tex.imageView = s.tex[0].view;
		Rayleigh_tex.sampler = m_samplers[s.tex[0].sampler_type];

		VkDescriptorImageInfo Mie_tex;
		Mie_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Mie_tex.imageView = s.tex[1].view;
		Mie_tex.sampler = m_samplers[s.tex[1].sampler_type];

		VkDescriptorImageInfo transmittance_tex;
		transmittance_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		transmittance_tex.imageView = s.tex[2].view;
		transmittance_tex.sampler = m_samplers[s.tex[2].sampler_type];

		std::array<VkWriteDescriptorSet, 3> write = {};
		write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[0].descriptorCount = 1;
		write[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[0].dstArrayElement = 0;
		write[0].dstBinding = 0;
		write[0].dstSet = s.ds;
		write[0].pImageInfo = &Rayleigh_tex;

		write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[1].descriptorCount = 1;
		write[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[1].dstArrayElement = 0;
		write[1].dstBinding = 1;
		write[1].dstSet = s.ds;
		write[1].pImageInfo = &Rayleigh_tex;

		write[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[2].descriptorCount = 1;
		write[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[2].dstArrayElement = 0;
		write[2].dstBinding = 2;
		write[2].dstSet = s.ds;
		write[2].pImageInfo = &transmittance_tex;

		vkUpdateDescriptorSets(m_base.device, write.size(), write.data(), 0, nullptr);
	}

	return s;
}

void resource_manager::destroy(sky&& s)
{
	vkFreeDescriptorSets(m_base.device, m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKY].pools[s.pool_index], 1, &s.ds);
	++m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_SKY].availability[s.pool_index];

	for (auto& t : s.tex)
	{
		vkDestroyImageView(m_base.device, t.view, m_alloc);
		vkDestroyImage(m_base.device, t.image, m_alloc);
		vkFreeMemory(m_base.device, t.memory, m_alloc);
	}
}

template<>
terrain resource_manager::build<RESOURCE_TYPE_TERRAIN>(const std::string& filename, const glm::uvec2& terrain_image_size)
{
	//variables
	uint32_t mip_level_count=8;
	glm::uvec2 level0_image_size = {};
	glm::uvec2 level0_tile_size = {};

	terrain t;

	//open files
	t.files_count = mip_level_count;
	t.files = new std::ifstream[mip_level_count];
	for (uint32_t i = 0; i < mip_level_count; ++i)
	{
		std::string s(filename);
		s = s + std::to_string(i) + ".terr";
		t.files[i].open(s.c_str());
		if (!t.files[i].is_open())
			throw std::runtime_error("failed to open file: " + s);
	}

	t.tex.mip_level_count = mip_level_count;
	t.tex.tile_count = level0_image_size / level0_tile_size;

	t.tex.tiles.resize(t.tex.tile_count.x);
	for (auto& v : t.tex.tiles)
	{
		v.resize(t.tex.tile_count.y);
		for (auto& t : v)
			t.current_min_mip_level = mip_level_count;
	}


	//create image
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.width = level0_image_size.x;
		image.extent.height = level0_image_size.y;
		image.extent.depth = 1;
		image.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
		image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = mip_level_count;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		if (vkCreateImage(m_base.device, &image, m_alloc, &t.tex.image) != VK_SUCCESS)
			throw std::runtime_error("failed to create image");
	}

	//calculate page size and allocate dummy page and mip tail
	{
		uint32_t mr_count;
		vkGetImageSparseMemoryRequirements(m_base.device, t.tex.image, &mr_count, nullptr);
		assert(mr_count == 1);
		VkSparseImageMemoryRequirements sparse_mr;
		vkGetImageSparseMemoryRequirements(m_base.device, t.tex.image, &mr_count, &sparse_mr);

		if (sparse_mr.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_NONSTANDARD_BLOCK_SIZE_BIT)
			throw std::runtime_error("nonstandard block size support for sparse images!");
		//block size should be 128x128

		t.tex.page_size.x = sparse_mr.formatProperties.imageGranularity.width;
		t.tex.page_size.y = sparse_mr.formatProperties.imageGranularity.height;
		t.tex.page_size_in_bytes = t.tex.page_size.x*t.tex.page_size.y * sizeof(glm::vec4);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, t.tex.image, &mr);
		t.tex.page_size_in_bytes = calc_offset(static_cast<size_t>(mr.alignment), t.tex.page_size_in_bytes);
		t.tex.mip_tail_size = static_cast<size_t>(calc_offset(mr.alignment, sparse_mr.imageMipTailSize));
		t.tex.mip_tail_offset = static_cast<size_t>(sparse_mr.imageMipTailOffset);

		size_t mem_size = t.tex.page_size_in_bytes + t.tex.mip_tail_size;

		VkMemoryAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc.allocationSize = mem_size;
		alloc.memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(m_base.device, &alloc, m_alloc, &t.tex.dummy_page_and_mip_tail) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate memory or dummy page and mip tail!");

		//set pool
		t.pool.set_alloc_info(m_base.device, m_alloc, alloc.memoryTypeIndex, 64, t.tex.page_size_in_bytes, false);
	}

	//bound dummy page, mip tail, and transition to shader read only optimal layout
	{
		//mip tail bind
		VkSparseMemoryBind mip_tail = {};
		mip_tail.memory = t.tex.dummy_page_and_mip_tail;
		mip_tail.memoryOffset = t.tex.page_size_in_bytes;
		mip_tail.resourceOffset = t.tex.mip_tail_offset;
		mip_tail.size = t.tex.mip_tail_size;

		VkSparseImageOpaqueMemoryBindInfo mip_tail_bind = {};
		mip_tail_bind.bindCount = 1;
		mip_tail_bind.image = t.tex.image;
		mip_tail_bind.pBinds = &mip_tail;

		size_t pages_count = 0;
		for (const auto& s : t.tex.tile_size_in_pages)
			pages_count += (s.x*s.y);
		pages_count *= (t.tex.tile_count.x*t.tex.tile_count.y);
		std::vector<VkSparseImageMemoryBind> page_binds(pages_count);

		size_t page_index = 0;
		for (uint32_t mip_level = 0; mip_level < mip_level_count; ++mip_level)
		{
			for (size_t i = 0; i < t.tex.tile_count.x; ++i)
			{
				for (size_t j = 0; j < t.tex.tile_count.y; ++j)
				{
					//at tile(i,j) in mip level mip_level
					VkOffset3D tile_offset =
					{
						i*t.tex.tile_size_in_pages[mip_level].x*t.tex.page_size.x,
						j*t.tex.tile_size_in_pages[mip_level].y*t.tex.page_size.y,
						0
					};

					for (size_t u = 0; u < t.tex.tile_size_in_pages[mip_level].x; ++u)
					{
						for (size_t v = 0; v < t.tex.tile_size_in_pages[mip_level].y; ++v)
						{
							//at page (u,v)
							VkOffset3D page_offset_in_tile =
							{
								u*t.tex.page_size.x,
								v*t.tex.page_size.y,
								0
							};
							page_binds[page_index].memory = t.tex.dummy_page_and_mip_tail;
							page_binds[page_index].memoryOffset = 0;
							page_binds[page_index].offset =
							{
								tile_offset.x + page_offset_in_tile.x,
								tile_offset.y + page_offset_in_tile.y,
								0
							};
							page_binds[page_index].extent =
							{
								t.tex.page_size.x,
								t.tex.page_size.y,
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
		}
		VkSparseImageMemoryBindInfo image_bind;
		image_bind.bindCount = page_binds.size();
		image_bind.pBinds = page_binds.data();
		image_bind.image = t.tex.image;

		VkBindSparseInfo bind_info = {};
		bind_info.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
		bind_info.imageOpaqueBindCount = 1;
		bind_info.pImageOpaqueBinds = &mip_tail_bind;
		bind_info.imageBindCount = 1;
		bind_info.pImageBinds = &image_bind;

		//VkSemaphoreCreateInfo s = {};
		//s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		//VkSemaphore binding_finished_s;
		//if (vkCreateSemaphore(m_base.device, &s, m_alloc, &binding_finished_s) != VK_SUCCESS)
		//	throw std::runtime_error("failed to create semaphore!");

		//bind_info.signalSemaphoreCount = 1;
		//bind_info.pSignalSemaphores = &binding_finished_s;

		vkQueueBindSparse(m_base.graphics_queue, 1, &bind_info, VK_NULL_HANDLE);

		//transition to shader read only optimal
		VkImageMemoryBarrier b = {};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.image = t.tex.image;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b.srcAccessMask = 0;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp_build);

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &b);

		end_single_time_command_buffer(m_base.device, m_cp_build, m_base.graphics_queue, cb);

		//vkDestroySemaphore(m_base.device, binding_finished_s, m_alloc);
	}

	//create image view
	{
		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = t.tex.image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.levelCount = mip_level_count;

		if (vkCreateImageView(m_base.device, &view, m_alloc, &t.tex.view) != VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}


	//should create descriptor set!!!

	return t;
}




void resource_manager::destroy(terrain&& t)
{
	/*vkFreeDescriptorSets(m_base.device, m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN].pools[t.pool_index], 1, &t.ds);
	++m_dpps[DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN].availability[t.pool_index];

	vkDestroyImageView(m_base.device, t.tex.view, m_alloc);
	vkDestroyImage(m_base.device, t.tex.image, m_alloc);
	vkFreeMemory(m_base.device, t.tex.memory, m_alloc);*/
}

template<>
terrain_tile resource_manager::build<RESOURCE_TYPE_TERRAIN_TILE>(unique_id terrain_id, uint32_t mip_level, const glm::uvec2& tile_id)
{
	auto& terr = std::get<RESOURCE_TYPE_TERRAIN>(m_resources_ready)[terrain_id];
	auto& terr_tile_mipmaps = terr.get_terrain_tile_mipmaps(tile_id);
	uint32_t new_mip_level = terr_tile_mipmaps.min_mip_level - 1u;

	terrain_tile tt;
	tt.terrain_id = terrain_id;
	tt.mip_level = new_mip_level;
	tt.tile_id = tile_id;

	auto& file = terr.files[new_mip_level];
	glm::uvec2 tile_size_in_pages = terr.tile_size_in_pages[new_mip_level];
	glm::uvec2 page_size = terr.tex.page_size;
	glm::uvec2 file_size = terr.tile_count*tile_size_in_pages*page_size;
	glm::uvec2 tile_offset2D_in_file = tile_id*tile_size_in_pages*page_size;
	
	tt.pages.resize(tile_size_in_pages.x);
	std::vector<std::vector<device_memory_pool::cell_info>> staging_pages(tile_size_in_pages.x);
	for (auto& v : tt.pages)
		v.resize(tile_size_in_pages.y);
	for (auto& v : staging_pages)
		v.resize(tile_size_in_pages.y);

	size_t tile_offset_in_bytes = (tile_offset2D_in_file.y*file_size.x + tile_offset2D_in_file.x) * sizeof(glm::vec4);
	for (uint32_t i = 0; i < tile_size_in_pages.x; ++i)
	{
		for (uint32_t j = 0; j < tile_size_in_pages.y; ++j)
		{
			tt.pages[i][j] = terr.page_pool.pop_cell();
			staging_pages[i][j] = terr.staging_buffer_memory_pool.pop_cell();

			char* staging_page_data = terr.staging_buffer_memory_pool.get_pointer(staging_pages[i][j]);

			size_t page_offset_in_bytes = tile_offset_in_bytes
				+ (file_size.x*page_size.y*j + i*page_size.x) * sizeof(glm::vec4);

			for (uint32_t k = 0; k < page_size.y; ++k)
			{
				size_t offset_in_bytes = page_offset_in_bytes + k*file_size.x * sizeof(glm::vec4);
				file.seekg(offset_in_bytes);
				file.read(staging_page_data, page_size.x * sizeof(glm::vec4));
				staging_page_data += (page_size.x * sizeof(glm::vec4));
			}
		}
	}

	//bind pages to virtual texture and staging buffer
	size_t page_count = tile_size_in_pages.x*tile_size_in_pages.y;
	std::vector<VkSparseImageMemoryBind> image_binds(page_count);
	std::vector<VkSparseMemoryBind> staging_buffer_binds(page_count);
	size_t index = 0;
	for (uint32_t i = 0; i < tile_size_in_pages.x; ++i)
	{
		for (uint32_t j = 0; j < tile_size_in_pages.y; ++j)
		{
			auto& b = image_binds[index];
			b.extent = { page_size.x, page_size.y, 1 };
			b.offset = { tile_offset2D_in_file.x + i*page_size.x, tile_offset2D_in_file.y + j*page_size.y, 0 };
			b.memory = terr.page_pool.get_memory(tt.pages[i][j]);
			b.memoryOffset = terr.page_pool.get_offset(tt.pages[i][j]);
			b.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresource.arrayLayer = 0;
			b.subresource.mipLevel = new_mip_level;

			auto& s = staging_buffer_binds[index];
			s.memory = terr.staging_buffer_memory_pool.get_memory(staging_pages[i][j]);
			s.memoryOffset = terr.staging_buffer_memory_pool.get_offset(staging_pages[i][j]);
			s.resourceOffset = index*terr.tex.page_size_in_bytes;
			s.size = terr.tex.page_size_in_bytes;
			++index;
		}
	}

	VkSparseImageMemoryBindInfo image_bind_info = {};
	image_bind_info.bindCount = page_count;
	image_bind_info.image = terr.tex.image;
	image_bind_info.pBinds = image_binds.data();

	VkSparseBufferMemoryBindInfo buffer_bind_info = {};
	buffer_bind_info.bindCount = page_count;
	buffer_bind_info.buffer = terr.staging_buffer;
	buffer_bind_info.pBinds = staging_buffer_binds.data();

	VkBindSparseInfo bind_info = {};
	bind_info.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
	bind_info.imageBindCount = 1;
	bind_info.pImageBinds = &image_bind_info;
	bind_info.bufferBindCount = 1;
	bind_info.pBufferBinds = &buffer_bind_info;

	VkSemaphoreCreateInfo s = {};
	s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkSemaphore memory_bind_finished_s;
	if (vkCreateSemaphore(m_base.device, &s, m_alloc, &memory_bind_finished_s) != VK_SUCCESS)
		throw std::runtime_error("failed to create semaphore!");
	bind_info.signalSemaphoreCount = 1;
	bind_info.pSignalSemaphores = &memory_bind_finished_s;

	vkQueueBindSparse(m_base.graphics_queue, 1, &bind_info, VK_NULL_HANDLE);


	//copy from staging buffers
	std::vector<VkBufferImageCopy> regions(page_count);
	index = 0;
	for (uint32_t i = 0; i < tile_size_in_pages.x; ++i)
	{
		for (uint32_t j = 0; j < tile_size_in_pages.y; ++j)
		{
			auto& r = regions[index];
			r.bufferOffset= index*terr.tex.page_size_in_bytes;
			r.imageOffset= { tile_offset2D_in_file.x + i*page_size.x, tile_offset2D_in_file.y + j*page_size.y, 0 };
			r.imageExtent= { page_size.x, page_size.y, 1 };
			r.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			r.imageSubresource.baseArrayLayer = 0;
			r.imageSubresource.layerCount = 1;
			r.imageSubresource.mipLevel = new_mip_level;
			
			++index;
		}
	}
	VkCommandBuffer cb;
	VkCommandBufferAllocateInfo cb_alloc = {};
	cb_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cb_alloc.commandBufferCount = 1;
	cb_alloc.commandPool = m_cp_build;
	cb_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	if (vkAllocateCommandBuffers(m_base.device, &cb_alloc, &cb) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate command buffer!");

	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS)
		throw std::runtime_error("failed to begin command buffer!");
	
	vkCmdCopyBufferToImage(cb, terr.staging_buffer, terr.tex.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, page_count, regions.data());

	if (vkEndCommandBuffer(cb) != VK_SUCCESS)
		throw std::runtime_error("failed to end command buffer!");

	VkFenceCreateInfo f = {};
	f.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence copy_finished_f;
	if (vkCreateFence(m_base.device, &f, m_alloc, &copy_finished_f) != VK_SUCCESS)
		throw std::runtime_error("failed to create fence!");

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers=&cb;
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	submit.waitSemaphoreCount = 1;
	submit.pWaitDstStageMask = &wait_stage;
	submit.pWaitSemaphores = &memory_bind_finished_s;
	if (vkQueueSubmit(m_base.graphics_queue, 1, &submit, copy_finished_f) != VK_SUCCESS)
		throw std::runtime_error("failed to submit command buffer!");
	vkWaitForFences(m_base.device, 1, &copy_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());

	vkDestroySemaphore(m_base.device, memory_bind_finished_s, m_alloc);
	vkDestroyFence(m_base.device, copy_finished_f, m_alloc);
	vkFreeCommandBuffers(m_base.device, m_cp_build, 1, &cb);

	for (uint32_t i = 0; i < tile_size_in_pages.x; ++i)
	{
		for (uint32_t j = 0; j < tile_size_in_pages.y; ++j)
		{
			terr.staging_buffer_memory_pool.push_cell(staging_pages[i][j]);
		}
	}

	return tt;
}

void resource_manager::destroy(terrain_tile&& tt)
{
	auto& terr = std::get<RESOURCE_TYPE_TERRAIN>(m_resources_ready)[tt.terrain_id];
	auto& terr_tile_mipmaps = terr.get_terrain_tile_mipmaps(tt.tile_id);

	uint32_t mip_level = terr_tile_mipmaps.min_mip_level;

	for (uint32_t i = 0; i < tt.pages.size(); ++i)
	{
		for (uint32_t j = 0; j < tt.pages[i].size(); ++j)
		{
			terr.page_pool.push_cell(tt.pages[i][j]);
		}
	}
}