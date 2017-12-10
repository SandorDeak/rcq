#include "resource_manager.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "os_memory.h"


using namespace rcq;

resource_manager* resource_manager::m_instance = nullptr;

void load_mesh(const std::string& file_name, std::vector<vertex>& vertices, std::vector<uint32_t>& indices,
	bool make_vertex_ext, std::vector<vertex_ext>& vertices_ext);

resource_manager::resource_manager(const base_info& info) : m_base(info)
{
	create_memory_resources_and_containers();
	create_samplers();
	create_dsls();
	create_dp_pools();
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

}


resource_manager::~resource_manager()
{
	/*m_should_end_build = true;
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
	check_resource_leak(std::make_index_sequence<RESOURCE_TYPE_COUNT>());*/
}

void resource_manager::build_loop()
{
	while (!m_should_end_build)
	{
		if (m_build_queue.empty())
		{
			std::unique_lock<std::mutex> lock;
			while (m_build_queue.empty())
				m_build_queue_cv.wait(lock);
		}

		base_resource_build_info* info = m_build_queue.front();
		switch (info->resource_type)
		{
		case 0:
			build<0>(info->base_res, info->data);
			break;
		case 1:
			build<1>(info->base_res, info->data);
			break;
		case 2:
			build<2>(info->base_res, info->data);
			break;
		case 3:
			build<3>(info->base_res, info->data);
			break;
		case 4:
			build<4>(info->base_res, info->data);
		case 5:
			build<5>(info->base_res, info->data);
			break;
		}
		static_assert(6 == RES_TYPE_COUNT);

		m_build_queue.pop();

	}
}

void resource_manager::destroy_loop()
{
	while (!m_should_end_destroy)
	{
		if (m_destroy_queue.empty())
		{
			std::unique_lock<std::mutex> lock(m_destroy_queue_mutex);
			while (m_destroy_queue.empty())
				m_destroy_queue_cv.wait(lock);
		}

		base_resource* base_res = *m_destroy_queue.front();

		while (!base_res->ready_bit.load());

		switch (base_res->res_type)
		{
		case 0:
			destroy<0>(base_res);
			break;
		case 1:
			destroy<1>(base_res);
			break;
		case 2:
			destroy<2>(base_res);
			break;
		case 3:
			destroy<3>(base_res);
			break;
		case 4:
			destroy<4>(base_res);
			break;
		case 5:
			destroy<5>(base_res);
			break;
		}
		static_assert(6 == RES_TYPE_COUNT);

		m_destroy_queue.pop();
	}
}

void resource_manager::init(const base_info& info, size_t place)
{

	assert(m_instance == nullptr);
	m_instance = new(reinterpret_cast<void*>(place)) resource_manager(info);
}

void resource_manager::destroy()
{
	assert(m_instance != nullptr);

	m_instance->~resource_manager;
	m_instance = nullptr;
}

void resource_manager::begin_build_cb()
{
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	assert(vkBeginCommandBuffer(m_build_cb, &begin_info)==VK_SUCCESS);
}
void resource_manager::end_build_cb(const VkSemaphore* wait_semaphores, const VkPipelineStageFlags* wait_flags,
	uint32_t wait_count)
{
	vkEndCommandBuffer(m_build_cb);

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &m_build_cb;
	submit.waitSemaphoreCount = wait_count;
	submit.pWaitDstStageMask = wait_flags;
	submit.pWaitSemaphores = wait_semaphores;

	vkResetFences(m_base.device, 1, &m_build_f);
	assert(vkQueueSubmit(m_base.transfer_queue, 1, &submit, m_build_f) == VK_SUCCESS);
}

void resource_manager::wait_for_build_fence()
{
	assert(vkWaitForFences(m_base.device, 1, &m_build_f, VK_TRUE, ~0) == VK_SUCCESS);
	vkResetFences(m_base.device, 1, &m_build_f);
}

void resource_manager::create_memory_resources_and_containers()
{
	const uint64_t HOST_MEMORY_SIZE = 256 * 1024 * 1024;
	m_host_memory.init(HOST_MEMORY_SIZE, MAX_ALIGNMENT, &OS_MEMORY);

	m_resource_pool.init(sizeof(base_resource), alignof(base_resource), &m_host_memory);
	m_vk_alloc.init(&m_host_memory);

	m_vk_mappable_memory.init(m_base.device, find_memory_type(m_base.physical_device, ~0,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), false, &m_vk_alloc);

	m_mappable_memory.init(256 * 1024 * 1024, 256, &m_vk_mappable_memory, &m_host_memory);

	m_vk_device_memory.init(m_base.device, find_memory_type(m_base.physical_device, ~0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
		false, &m_vk_alloc);
	m_device_memory.init(2 * 1024 * 1024 * 1024, 256, &m_vk_device_memory, &m_host_memory);

	m_build_queue.init(&m_host_memory);
	m_build_queue.init_buffer();

	m_destroy_queue.init(&m_host_memory);
	m_destroy_queue.init_buffer();
}

void resource_manager::create_dp_pools()
{
	const uint32_t TR_POOL_CAPACITY = 64;
	const uint32_t MAT_OPAQUE_CAPACITY = 64;

	//create tr dp_pool
	{
		auto& pool = m_dp_pools[DSL_TYPE_TR];

		new(&pool) dp_pool(1, TR_POOL_CAPACITY, m_base.device, &m_host_memory);
		pool.sizes()[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.sizes()[0].descriptorCount = TR_POOL_CAPACITY;
	}

	//create sky pool
	{
		auto& pool = m_dp_pools[DSL_TYPE_SKY];
		new(&pool) dp_pool(1, 1, m_base.device, &m_host_memory);
		pool.sizes()[0].descriptorCount = 3;
		pool.sizes()[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	}

	//create water pool
	{
		auto& pool = m_dp_pools[DSL_TYPE_WATER];
		new(&pool) dp_pool(2, 1, m_base.device, &m_host_memory);
		pool.sizes()[0].descriptorCount = 1;
		pool.sizes()[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.sizes()[1].descriptorCount = 1;
		pool.sizes()[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool.sizes()[2].descriptorCount = 2;
		pool.sizes()[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	}

	//create terrain pool
	{
		auto& pool = m_dp_pools[DSL_TYPE_TERRAIN];
		new(&pool) dp_pool(5, 1, m_base.device, &m_host_memory);
		pool.sizes()[0].descriptorCount = 1;
		pool.sizes()[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool.sizes()[1].descriptorCount = 1;
		pool.sizes()[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		pool.sizes()[2].descriptorCount = 1;
		pool.sizes()[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.sizes()[3].descriptorCount = 1;
		pool.sizes()[3].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		pool.sizes()[4].descriptorCount = 1;
		pool.sizes()[4].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}

	//create mat opaque pool
	{
		auto& pool = m_dp_pools[DSL_TYPE_MAT_OPAQUE];
		new(&pool) dp_pool(2, MAT_OPAQUE_CAPACITY, m_base.device, &m_host_memory);
		pool.sizes()[0].descriptorCount = MAT_OPAQUE_CAPACITY;
		pool.sizes()[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.sizes()[1].descriptorCount = TEX_TYPE_COUNT*MAT_OPAQUE_CAPACITY;
		pool.sizes()[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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
void resource_manager::build<RES_TYPE_MESH>(base_resource* res, const char* build_info)
{
	const resource<RES_TYPE_MESH>::build_info* build = reinterpret_cast<const resource<RES_TYPE_MESH>::build_info*>(build_info);
	auto& _mesh = *reinterpret_cast<resource<RES_TYPE_MESH>*>(res);

	std::vector<vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<vertex_ext> vertices_ext;
	load_mesh(build->filename, vertices, indices, build->calc_tb, vertices_ext);

	_mesh.size = indices.size();

	uint64_t vb_size = sizeof(vertex)*vertices.size();
	uint64_t ib_size = sizeof(uint32_t)*indices.size();
	uint64_t veb_size = build->calc_tb? sizeof(vertex_ext)*vertices_ext.size():0;

	uint64_t sb_size = vb_size + ib_size + veb_size; //staging buffer size

	uint64_t vb_staging = m_mappable_memory.allocate(vb_size, 1);
	uint64_t ib_staging = m_mappable_memory.allocate(ib_size, 1);
	uint64_t veb_staging = build->calc_tb ? m_mappable_memory.allocate(veb_size, 1) : 0;


	//create vertex buffer
	VkBufferCreateInfo vb_info = {};
	vb_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vb_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vb_info.size = vb_size;
	vb_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if (vkCreateBuffer(m_base.device, &vb_info, m_vk_alloc, &_mesh.vb) != VK_SUCCESS)
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

	if (vkCreateBuffer(m_base.device, &ib_info, m_vk_alloc, &_mesh.ib) != VK_SUCCESS)
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
	if (build->calc_tb)
	{
		VkBufferCreateInfo veb_info = {};
		veb_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		veb_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		veb_info.size = veb_size;
		veb_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		if (vkCreateBuffer(m_base.device, &veb_info, m_vk_alloc, &_mesh.veb) != VK_SUCCESS)
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
	uint64_t vb_offset = m_device_memory.allocate(vb_size, vb_mr.alignment);
	uint64_t ib_offset = m_device_memory.allocate(ib_size, ib_mr.alignment);
	uint64_t veb_offset = build->calc_tb ? m_device_memory.allocate(veb_size, veb_mr.alignment) : 0;

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
	vkBindBufferMemory(m_base.device, _mesh.vb, m_device_memory.handle(), vb_offset);
	vkBindBufferMemory(m_base.device, _mesh.ib, m_device_memory.handle(), ib_offset);
	if (build->calc_tb)
		vkBindBufferMemory(m_base.device, _mesh.veb, m_device_memory.handle(), veb_offset);

	//fill staging buffer

	void* raw_data;
	vkMapMemory(m_base.device, m_mappable_memory.handle(), vb_staging, sb_size, 0, &raw_data);
	char* data = static_cast<char*>(raw_data);
	memcpy(data, vertices.data(), vb_size);
	vkUnmapMemory(m_base.device, m_mappable_memory.handle());

	vkMapMemory(m_base.device, m_mappable_memory.handle(), ib_staging, ib_size, 0, &raw_data);
	data= static_cast<char*>(raw_data);
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
	vkCmdCopyBuffer(m_build_cb, m_staging_buffer, _mesh.vb, 1, &copy_region);

	copy_region.dstOffset = 0;
	copy_region.size = ib_size;
	copy_region.srcOffset = ib_staging;
	vkCmdCopyBuffer(m_build_cb, m_staging_buffer, _mesh.ib, 1, &copy_region);


	if (build->calc_tb)
	{
		copy_region.dstOffset = veb_offset;
		copy_region.size = veb_size;
		copy_region.srcOffset = veb_staging;
		vkCmdCopyBuffer(m_build_cb, m_staging_buffer, _mesh.veb, 1, &copy_region);
	}

	end_build_cb();
	wait_for_build_fence();
	
	m_mappable_memory.deallocate(vb_staging);
	m_mappable_memory.deallocate(ib_staging);
	if (build->calc_tb)
		m_mappable_memory.deallocate(veb_staging);
}


template<>
void resource_manager::build<RES_TYPE_MAT_OPAQUE>(base_resource* res, const char* build_info)
{
	const resource<RES_TYPE_MAT_OPAQUE>::build_info* build = reinterpret_cast<const resource<RES_TYPE_MAT_OPAQUE>::build_info*>(build_info);

	auto& mat = *reinterpret_cast<resource<RES_TYPE_MAT_OPAQUE>*>(res->data);


	begin_build_cb();

	//load textures
	uint64_t staging_buffer_offsets[TEX_TYPE_COUNT];
	uint32_t flag = 1;
	for (uint32_t i=0; i<TEX_TYPE_COUNT; ++i)
	{
		if ((flag & build->tex_flags) && build->texfiles[i] != "empty")
		{
			auto& tex = mat.texs[i];

			int width, height, channels;
			stbi_uc* pixels = stbi_load(build->texfiles[i], &width, &height, &channels, STBI_rgb_alpha);

			if (!pixels)
			{
				throw std::runtime_error("failed to load texture image");
			}

			uint64_t im_size = width*height * 4;

			staging_buffer_offsets[i] = m_mappable_memory.allocate(im_size, 1);
			void* staging_buffer_data = reinterpret_cast<void*>(m_mappable_memory.map(staging_buffer_offsets[i], im_size));
			memcpy(staging_buffer_data, pixels, im_size);
			m_mappable_memory.unmap();
			stbi_image_free(pixels);

			uint32_t mip_level_count = 0;
			uint32_t mip_level_size = std::max(width, height);
			while (mip_level_size != 0)
			{
				mip_level_size >>= 1;
				++mip_level_count;
			}

			//create image
			{
				VkImageCreateInfo im = {};
				im.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				im.arrayLayers = 1;
				im.extent.width = static_cast<uint32_t>(width);
				im.extent.height = static_cast<uint32_t>(height);
				im.extent.depth = 1;
				im.format = VK_FORMAT_R8G8B8A8_UNORM;
				im.imageType = VK_IMAGE_TYPE_2D;
				im.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				im.mipLevels = mip_level_count;
				im.samples = VK_SAMPLE_COUNT_1_BIT;
				im.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				im.tiling = VK_IMAGE_TILING_OPTIMAL;
				im.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

				assert(vkCreateImage(m_base.device, &im, m_vk_alloc, &tex.image) == VK_SUCCESS);
			}

			//allocate memory for image
			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, tex.image, &mr);

			tex.offset = m_device_memory.allocate(mr.size, mr.alignment);

			vkBindImageMemory(m_base.device, tex.image, m_device_memory.handle(), tex.offset);

			//transition level0 layout to transfer dst optimal
			{

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

				vkCmdPipelineBarrier(
					m_build_cb,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);
			}
			//copy from staging buffer to level0
			{
				VkBufferImageCopy region = {};
				region.bufferImageHeight = 0;
				region.bufferRowLength = 0;
				region.bufferOffset = staging_buffer_offsets[0];
				region.imageExtent.depth = 1;
				region.imageExtent.width = width;
				region.imageExtent.height = height;
				region.imageOffset = { 0, 0, 0 };
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageSubresource.mipLevel = 0;

				vkCmdCopyBufferToImage(m_build_cb, m_staging_buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			}
			//fill mip levels
			for (uint32_t i = 0; i < mip_level_count - 1; ++i)
			{
				std::vector<VkImageMemoryBarrier> barriers(2);
				for (uint32_t j = 0; j < 2; ++j)
				{
					barriers[j].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					barriers[j].image = tex.image;
					barriers[j].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barriers[j].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barriers[j].oldLayout = j == 0 ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
					barriers[j].newLayout = j == 0 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					barriers[j].srcAccessMask = j == 0 ? VK_ACCESS_TRANSFER_WRITE_BIT : 0;
					barriers[j].dstAccessMask = j == 0 ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
					barriers[j].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					barriers[j].subresourceRange.baseArrayLayer = 0;
					barriers[j].subresourceRange.layerCount = 1;
					barriers[j].subresourceRange.baseMipLevel = i + j;
					barriers[j].subresourceRange.levelCount = 1;
				}
				vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr, 0, nullptr, 2, barriers.data());

				VkImageBlit blit = {};
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { width, height, 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
				blit.srcSubresource.mipLevel = i;

				width >>= 1;
				height >>= 1;

				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { width, height, 1 };
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;
				blit.dstSubresource.mipLevel = i + 1;

				vkCmdBlitImage(m_build_cb, tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &blit, VK_FILTER_LINEAR);
			}

			//transition all level layout to shader read only optimal
			std::array<VkImageMemoryBarrier, 2> barriers = {};
			for (uint32_t i = 0; i < 2; ++i)
			{
				barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barriers[i].srcAccessMask = i == 0 ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
				barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barriers[i].image = tex.image;
				barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barriers[i].oldLayout = i == 0 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barriers[i].subresourceRange.baseArrayLayer = 0;
				barriers[i].subresourceRange.baseMipLevel = i == 0 ? 0 : mip_level_count - 1;
				barriers[i].subresourceRange.layerCount = 1;
				barriers[i].subresourceRange.levelCount = i == 0 ? mip_level_count - 1 : 1;
			}

			vkCmdPipelineBarrier(
				m_build_cb,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				2, barriers.data()
			);

			//create image view
			{
				VkImageViewCreateInfo view = {};
				view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				view.format = VK_FORMAT_R8G8B8A8_UNORM;
				view.image = tex.image;
				view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				view.subresourceRange.baseArrayLayer = 0;
				view.subresourceRange.baseMipLevel = 0;
				view.subresourceRange.layerCount = 1;
				view.subresourceRange.levelCount = mip_level_count;
				view.viewType = VK_IMAGE_VIEW_TYPE_2D;

				assert(vkCreateImageView(m_base.device, &view, m_vk_alloc,
					&tex.view) == VK_SUCCESS);
			}

			//create sampler
			{
				VkSamplerCreateInfo s = {};
				s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				s.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				s.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				s.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				s.anisotropyEnable = VK_TRUE;
				s.compareEnable = VK_FALSE;
				s.maxAnisotropy = 16.f;
				s.magFilter = VK_FILTER_LINEAR;
				s.minFilter = VK_FILTER_LINEAR;
				s.minLod = 0.f;
				s.maxLod = static_cast<float>(mip_level_count - 1);
				s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				s.unnormalizedCoordinates = VK_FALSE;

				assert(vkCreateSampler(m_base.device, &s, m_vk_alloc, &tex.sampler) == VK_SUCCESS);
			}
		}
		else
		{
			staging_buffer_offsets[i] = 0;
			mat.texs[i].offset = 0;
		}
		flag = flag << 1;
	}

	//create material data buffer
	{
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer.size = sizeof(material_opaque_data);
		buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		assert(vkCreateBuffer(m_base.device, &buffer, m_vk_alloc, &mat.data_buffer) == VK_SUCCESS);
	}

	//allocate memory for material data buffer
	{
		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, mat.data_buffer, &mr);
		mat.data_offset = m_device_memory.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, mat.data_buffer, m_device_memory.handle(), mat.data_offset);
	}

	//copy to staging buffer
	uint64_t staging_buffer_offset = m_mappable_memory.allocate(sizeof(resource<RES_TYPE_MAT_OPAQUE>::data),
		alignof(resource<RES_TYPE_MAT_OPAQUE>::data));
	resource<RES_TYPE_MAT_OPAQUE>::data* data = reinterpret_cast<resource<RES_TYPE_MAT_OPAQUE>::data*>(
		m_mappable_memory.map(staging_buffer_offset, sizeof(resource<RES_TYPE_MAT_OPAQUE>::data)));

	data->color = build->color;
	data->flags = build->tex_flags;
	data->height_scale = build->height_scale;
	data->metal = build->metal;
	data->roughness = build->roughness;

	m_mappable_memory.unmap();


	//copy to material data buffer
	{
		VkBufferCopy region = {};
		region.dstOffset = 0;
		region.size = sizeof(resource<RES_TYPE_MAT_OPAQUE>::data);
		region.srcOffset = staging_buffer_offset;

		vkCmdCopyBuffer(m_build_cb, m_staging_buffer, mat.data_buffer, 1, &region);
	}

	end_build_cb();

	//allocate descriptor set
	{
		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &m_dsls[DSL_TYPE_MAT_OPAQUE];
		alloc_info.descriptorPool = m_dp_pools[DSL_TYPE_MAT_OPAQUE].use_dp(mat.dp_index);

		assert(vkAllocateDescriptorSets(m_base.device, &alloc_info, &mat.ds) == VK_SUCCESS);	
	}

	//update descriptor set
	{
		VkDescriptorBufferInfo buffer_info = {};
		buffer_info.buffer = mat.data_buffer;
		buffer_info.offset = 0;
		buffer_info.range = sizeof(material_opaque_data);

		std::array<VkDescriptorImageInfo, TEX_TYPE_COUNT> tex_info;
		std::array<VkWriteDescriptorSet, TEX_TYPE_COUNT + 1> write = {};

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
			if (flag & build->tex_flags)
			{
				tex_info[tex_info_index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				tex_info[tex_info_index].imageView = mat.texs[i].view;
				tex_info[tex_info_index].sampler = mat.texs[i].sampler;

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
	}

	wait_for_build_fence();
	
	res->ready_bit.store(true, std::memory_order_release);

	m_mappable_memory.deallocate(staging_buffer_offset);
	for (uint32_t i = 0; i < TEX_TYPE_COUNT; ++i)
	{
		if (staging_buffer_offsets[i] != 0)
			m_mappable_memory.deallocate(staging_buffer_offsets[i]);
	}
}

template<>
void resource_manager::build<RESOURCE_TYPE_TR>(base_resource* res, const char* build_info)
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

	wait_for_build_fence();
	res->ready_bit.store(true, std::memory_order_release);

	m_mappable_memory.deallocate(staging_buffer_offset);
}

template<>
void resource_manager::destroy<RES_TYPE_MESH>(base_resource* res)
{
	auto m = reinterpret_cast<resource<RES_TYPE_MESH>*>(res->data);

	vkDestroyBuffer(m_base.device, m->vb, m_vk_alloc);
	m_device_memory.deallocate(m->vb_offset);

	vkDestroyBuffer(m_base.device, m->ib, m_vk_alloc);
	m_device_memory.deallocate(m->ib_offset);

	if (m->veb_offset != 0)
	{
		vkDestroyBuffer(m_base.device, m->veb, m_vk_alloc);
		m_device_memory.deallocate(m->veb);
	}
}

template<>
void resource_manager::destroy<RES_TYPE_MAT_OPAQUE>(base_resource* res)
{
	auto mat = reinterpret_cast<resource<RES_TYPE_MAT_OPAQUE>*>(res->data);

	vkFreeDescriptorSets(m_base.device, m_dp_pools[DSL_TYPE_MAT_OPAQUE].stop_using_dp(mat->dp_index), 1, &mat->ds);


	for (size_t i = 0; i < TEX_TYPE_COUNT; ++i)
	{
		if (mat->texs[i].offset != 0)
		{
			vkDestroyImageView(m_base.device, mat->texs[i].view, m_vk_alloc);
			vkDestroyImage(m_base.device, mat->texs[i].image, m_vk_alloc);
			vkDestroySampler(m_base.device, mat->texs[i].sampler, m_vk_alloc);
			m_device_memory.deallocate(mat->texs[i].offset);
		}
	}
	
	vkDestroyBuffer(m_base.device, mat->data_buffer, m_vk_alloc);
	m_device_memory.deallocate(mat->data_offset);
}

template<>
void resource_manager::destroy<RES_TYPE_TR>(base_resource* res)
{
	auto tr = reinterpret_cast<resource<RES_TYPE_TR>*>(res->data);

	vkFreeDescriptorSets(m_base.device, m_dp_pools[DSL_TYPE_TR].stop_using_dp(tr->dp_index), 1, &tr->ds);
	vkDestroyBuffer(m_base.device, tr->data_buffer, m_vk_alloc);
	m_device_memory.deallocate(tr->data_offset);
}


void rcq::resource_manager::create_samplers()
{
	//repeate
	{
		VkSamplerCreateInfo s = {};
		s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		s.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		s.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		s.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		s.anisotropyEnable = VK_TRUE;
		s.magFilter = VK_FILTER_LINEAR;
		s.minFilter = VK_FILTER_LINEAR;
		s.maxAnisotropy = 16;
		s.unnormalizedCoordinates = VK_FALSE;
		s.compareEnable = VK_FALSE;
		s.compareOp = VK_COMPARE_OP_ALWAYS;
		s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		s.mipLodBias = 0.f;
		s.minLod = 0.f;
		s.maxLod = 0.f;

		assert(vkCreateSampler(m_base.device, &s, m_vk_alloc, &m_samplers[SAMPLER_TYPE_REPEATE]) == VK_SUCCESS);
	}

	//clamp
	{
		VkSamplerCreateInfo s = {};
		s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		s.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.anisotropyEnable = VK_FALSE;
		s.maxAnisotropy = 16.f;
		s.compareEnable = VK_FALSE;
		s.magFilter = VK_FILTER_LINEAR;
		s.maxLod = 0.f;
		s.minFilter = VK_FILTER_LINEAR;
		s.minLod = 0.f;
		s.mipLodBias = 0.f;
		s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		assert(vkCreateSampler(m_base.device, &s, m_vk_alloc, &m_samplers[SAMPLER_TYPE_CLAMP]) == VK_SUCCESS);
	}
}

void resource_manager::create_dsls()
{
	//create transform dsl
	{
		VkDescriptorSetLayoutBinding data = {};
		data.binding = 0;
		data.descriptorCount = 1;
		data.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		data.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = &data;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_TR]) == VK_SUCCESS);
	}

	//create material opaque dsl
	{
		VkDescriptorSetLayoutBinding tex = {};
		tex.descriptorCount = 1;
		tex.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		tex.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding bindings[TEX_TYPE_COUNT + 1];
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		for (size_t i = 1; i < TEX_TYPE_COUNT + 1; ++i)
		{
			bindings[i].binding = i;
			bindings[i].descriptorCount = 1;
			bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}


		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = TEX_TYPE_COUNT + 1;
		dsl.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_MAT_OPAQUE]) == VK_SUCCESS);
	}

	//create sky dsl
	{
		VkDescriptorSetLayoutBinding bindings[3];
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
		dsl.bindingCount = 3;
		dsl.pBindings = bindings;
		
		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_SKY]) == VK_SUCCESS);
	}

	//create terrain dsl
	{
		VkDescriptorSetLayoutBinding bindings[3];
		bindings[0].binding = 0;
		bindings[0].descriptorCount=1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
			VK_SHADER_STAGE_VERTEX_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

		bindings[2].binding = 2;
		bindings[2].descriptorCount = 1;
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		bindings[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 3;
		dsl.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_TERRAIN]) == VK_SUCCESS);
	}

	//create terrain compute dsl
	{
		VkDescriptorSetLayoutBinding bindings[2];
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 2;
		dsl.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_TERRAIN_COMPUTE]) == VK_SUCCESS);
	}

	//create water dsl
	{
		VkDescriptorSetLayoutBinding bindings[1];
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_WATER]) == VK_SUCCESS);
	}

	//create water compute
	{
		VkDescriptorSetLayoutBinding bindings[3];
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		bindings[2].binding = 2;
		bindings[2].descriptorCount = 1;
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutCreateInfo dsl_info = {};
		dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl_info.bindingCount = 3;
		dsl_info.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl_info, m_vk_alloc, &m_dsls[DSL_TYPE_WATER_COMPUTE]) == VK_SUCCESS);
	}
}

void resource_manager::create_command_pool()
{
	VkCommandPoolCreateInfo cp = {};
	cp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cp.queueFamilyIndex = m_base.queue_families.graphics_family;
	cp.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	assert(vkCreateCommandPool(m_base.device, &cp, m_vk_alloc, &m_build_cp) == VK_SUCCESS);
}

template<>
void resource_manager::build<RESOURCE_TYPE_SKY>(base_resource* res, const char* build_info)
{
	resource<RES_TYPE_SKY>* s = reinterpret_cast<resource<RES_TYPE_SKY>*>(res->data);
	const auto build = reinterpret_cast<const resource<RES_TYPE_SKY>::build_info*>(build_info);

	const char* postfix[3] = { "_Rayleigh.sky", "_Mie.sky", "_transmittance.sky" };

	uint64_t sky_im_size = build->sky_image_size.x*build->sky_image_size.y*build->sky_image_size.z * sizeof(glm::vec4);
	uint64_t transmittance_im_size = build->transmittance_image_size.x*build->transmittance_image_size.y*sizeof(glm::vec4);

	uint64_t staging_buffer_offsets[3];

	begin_build_cb();

	for (uint32_t i = 0; i < 3; ++i)
	{

		//load image from file to staging buffer;
		char filename[256];
		strcpy(filename, build->filename);
		strcat(filename, postfix[i]);
		
		uint64_t size = i == 2 ? transmittance_im_size : sky_im_size;

		staging_buffer_offsets[i] = m_mappable_memory.allocate(size, 1);
		char* data = reinterpret_cast<char*>(m_mappable_memory.map(staging_buffer_offsets[i], size));
		read_file2(filename, data);
		m_mappable_memory.unmap();

		VkExtent3D extent;
		extent.width = i == 2 ? build->transmittance_image_size.x : build->sky_image_size.x;
		extent.height = i == 2 ? build->transmittance_image_size.y : build->sky_image_size.y;
		extent.depth = i == 2 ? 1 : build->sky_image_size.z;

		//create texture
		{
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.extent = extent;
			image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			image.arrayLayers = 1;
			image.imageType = i==2 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
			image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image.mipLevels = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &s->tex[i].image) == VK_SUCCESS);

			VkMemoryRequirements mr;
			vkGetImageMemoryRequirements(m_base.device, s->tex[i].image, &mr);
			s->tex[i].offset = m_device_memory.allocate(mr.size, mr.alignment);
			vkBindImageMemory(m_base.device, s->tex[i].image, m_device_memory.handle(), s->tex[i].offset);

			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			view.image = s->tex[i].image;
			view.viewType = i==2 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &s->tex[i].view) == VK_SUCCESS);

			s->tex[i].sampler = m_samplers[SAMPLER_TYPE_CLAMP];
		}


		//transition to transfer dst optimal
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = s->tex[i].image;
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

			vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
				0, nullptr, 1, &b);
		}

		//copy from staging buffer
		{
			VkBufferImageCopy region = {};
			region.bufferImageHeight = 0;
			region.bufferOffset = staging_buffer_offsets[i];
			region.bufferRowLength = 0;
			region.imageExtent = extent;
			region.imageOffset = { 0,0,0 };
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.mipLevel = 0;

			vkCmdCopyBufferToImage(m_build_cb, m_staging_buffer, s->tex[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		//transition to shader read only optimal
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = s->tex[i].image;
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

			vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
				0, nullptr, 1, &b);
		}
		
	}

	end_build_cb();

	//allocate descriptor set
	{
		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &m_dsls[DSL_TYPE_SKY];
		alloc_info.descriptorPool = m_dp_pools[DSL_TYPE_SKY].use_dp(s->dp_index);

		assert(vkAllocateDescriptorSets(m_base.device, &alloc_info, &s->ds) == VK_SUCCESS);
	}

	//update ds
	{
		VkDescriptorImageInfo Rayleigh_tex;
		Rayleigh_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Rayleigh_tex.imageView = s->tex[0].view;
		Rayleigh_tex.sampler = s->tex[0].sampler;

		VkDescriptorImageInfo Mie_tex;
		Mie_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Mie_tex.imageView = s->tex[1].view;
		Mie_tex.sampler = s->tex[1].sampler;

		VkDescriptorImageInfo transmittance_tex;
		transmittance_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		transmittance_tex.imageView = s->tex[2].view;
		transmittance_tex.sampler = s->tex[2].sampler;

		std::array<VkWriteDescriptorSet, 3> write = {};
		write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[0].descriptorCount = 1;
		write[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[0].dstArrayElement = 0;
		write[0].dstBinding = 0;
		write[0].dstSet = s->ds;
		write[0].pImageInfo = &Rayleigh_tex;

		write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[1].descriptorCount = 1;
		write[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[1].dstArrayElement = 0;
		write[1].dstBinding = 1;
		write[1].dstSet = s->ds;
		write[1].pImageInfo = &Rayleigh_tex;

		write[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[2].descriptorCount = 1;
		write[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[2].dstArrayElement = 0;
		write[2].dstBinding = 2;
		write[2].dstSet = s->ds;
		write[2].pImageInfo = &transmittance_tex;

		vkUpdateDescriptorSets(m_base.device, write.size(), write.data(), 0, nullptr);
	}

	wait_for_build_fence();
	res->ready_bit.store(true, std::memory_order_release);

	for (uint32_t i = 0; i < 3; ++i)
		m_mappable_memory.deallocate(staging_buffer_offsets[i]);
}

template<>
void resource_manager::destroy<RES_TYPE_SKY>(base_resource* res)
{
	auto s = reinterpret_cast<resource<RES_TYPE_SKY>*>(res->data);

	vkFreeDescriptorSets(m_base.device, m_dp_pools[DSL_TYPE_SKY].stop_using_dp(s->dp_index), 1, &s->ds);

	for (uint32_t i=0; i<3; ++i)
	{
		vkDestroyImageView(m_base.device, s->tex[i].view, m_vk_alloc);
		vkDestroyImage(m_base.device, s->tex[i].image, m_vk_alloc);
		m_device_memory.deallocate(s->tex[i].offset);
	}
}

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
		new(t->tex.files[i]) std::ifstream(filename, std::ios::binary);
		assert(t->tex.files[i]->is_open());
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
		t->tex.mip_tail_offset = m_device_memory_res.allocate(sparse_mr.imageMipTailSize, mr.alignment);
		t->tex.dummy_page_offset = m_device_memory_res.allocate(dummy_page_size, mr.alignment);

		//mip tail bind
		VkSparseMemoryBind mip_tail = {};
		mip_tail.memory = m_device_memory_res.handle();
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
							page_binds[page_index].memory = m_device_memory_res.handle();
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
		t->data_offset = m_device_memory_res.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, t->data_buffer, m_device_memory_res.handle(), t->data_offset);

		data_staging_buffer_offset = m_mappable_memory.allocate(sizeof(resource<RES_TYPE_TERRAIN>::data),
			alignof(resource<RES_TYPE_TERRAIN>::data));

		auto data = reinterpret_cast<resource<RES_TYPE_TERRAIN>::data*>(m_mappable_memory.map(data_staging_buffer_offset,
			sizeof(resource<RES_TYPE_TERRAIN>::data)));

		data->height_scale = build->size_in_meters.y;
		data->mip_level_count = static_cast<float>(build->mip_level_count);
		data->terrain_size_in_meters = { build->size_in_meters.x, build->size_in_meters.z };
		data->tile_count = static_cast<glm::ivec2>(build->level0_image_size / build->level0_tile_size);
		data->meter_per_tile_size_length = glm::vec2(build->size_in_meters.x, build->size_in_meters.z )/
			static_cast<glm::vec2>(build->level0_image_size/build->level0_tile_size);

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
		t->request_data_offset = m_device_memory_res.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, t->request_data_buffer, m_device_memory_res.handle(), t->request_data_offset);

		request_data_staging_buffer_offset = m_mappable_memory.allocate(sizeof(resource<RES_TYPE_TERRAIN>::request_data),
			alignof(resource<RES_TYPE_TERRAIN>::request_data));
		auto data = reinterpret_cast<resource<RES_TYPE_TERRAIN>::request_data*>(m_mappable_memory.map(request_data_staging_buffer_offset,
			sizeof(resource<RES_TYPE_TERRAIN>::request_data)));

		data->mip_level_count = static_cast<float>(build->mip_level_count);
		//data->request_count = 0;
		data->tile_size_in_meter = glm::vec2(build->size_in_meters.x, build->size_in_meters.z)/
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
			*m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN],
			*m_dsls[DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN_COMPUTE]
		};

		VkDescriptorSetAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pSetLayouts = dsls;
		alloc.descriptorSetCount = 2;
		alloc.descriptorPool = m_dp_pools[DSL_TYPE_TERRAIN]->use_dp(t->dp_index);
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


template<>
void resource_manager::destroy<RES_TYPE_TERRAIN>(base_resource* res)
{
	auto t = reinterpret_cast<resource<RES_TYPE_TERRAIN>*>(res->data);

	VkDescriptorSet dss[2] = { t->ds, t->request_ds };
	vkFreeDescriptorSets(m_base.device, m_dp_pools[DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN]->stop_using_dp(t->dp_index), 2, dss);

	vkDestroyImageView(m_base.device, t->tex.view, m_vk_alloc);
	vkDestroyImage(m_base.device, t->tex.image, m_vk_alloc);
	vkDestroySampler(m_base.device, t->tex.sampler, m_vk_alloc);
	m_device_memory_res.deallocate(t->tex.dummy_page_offset);
	m_device_memory_res.deallocate(t->tex.mip_tail_offset);

	vkDestroyBuffer(m_base.device, t->data_buffer, m_vk_alloc);
	m_device_memory_res.deallocate(t->data_offset);

	vkDestroyBuffer(m_base.device, t->request_data_buffer, m_vk_alloc);
	m_device_memory_res.deallocate(t->request_data_offset);

	/*vkDestroyBufferView(m_base.device, t.current_mip_levels_view, m_alloc);
	vkDestroyBuffer(m_base.device, t.current_mip_levels_buffer, m_alloc);
	vkFreeMemory(m_base.device, t.current_mip_levels_memory, m_alloc);

	vkDestroyBufferView(m_base.device, t.requested_mip_levels_view, m_alloc);
	vkDestroyBuffer(m_base.device, t.requested_mip_levels_buffer, m_alloc);
	vkFreeMemory(m_base.device, t.requested_mip_levels_memory, m_alloc);*/
}


template<>
void resource_manager::build<RES_TYPE_WATER>(base_resource* res, const char* build_info)
{
	auto w = reinterpret_cast<resource<RES_TYPE_WATER>*>(res->data);
	const auto build = reinterpret_cast<const resource<RES_TYPE_WATER>::build_info*>(build_info);

	w->grid_size_in_meters = build->grid_size_in_meters;

	uint32_t noise_size = resource<RES_TYPE_WATER>::GRID_SIZE*resource<RES_TYPE_WATER>::GRID_SIZE * sizeof(glm::vec4);

	uint64_t noise_staging_buffer_offset = m_mappable_memory.allocate(noise_size, 1);
	char* data = reinterpret_cast<char*>(m_mappable_memory.map(noise_staging_buffer_offset, noise_size));
	read_file2(build->filename, data);
	m_mappable_memory.unmap();

	//create noise image
	{
		VkImageCreateInfo im = {};
		im.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		im.arrayLayers = 1;
		im.extent = { GRID_SIZE, GRID_SIZE, 1 };
		im.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		im.imageType = VK_IMAGE_TYPE_2D;
		im.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		im.mipLevels = 1;
		im.samples = VK_SAMPLE_COUNT_1_BIT;
		im.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		im.tiling = VK_IMAGE_TILING_OPTIMAL;
		im.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

		assert(vkCreateImage(m_base.device, &im, m_vk_alloc, &w->noise.image) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, w->noise.image, &mr);
		w->noise.offset = m_device_memory_res.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, w->noise.image, m_device_memory_res.handle(), w->noise.offset);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = w->noise.image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &w->noise.view) == VK_SUCCESS);
	}

	//create height and grad texture
	{
		VkImageCreateInfo im = {};
		im.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		im.arrayLayers = 2;
		im.extent = { resource<RES_TYPE_WATER>::GRID_SIZE, resource<RES_TYPE_WATER>::GRID_SIZE, 1 };
		im.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		im.imageType = VK_IMAGE_TYPE_2D;
		im.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		im.mipLevels = 1;
		im.samples = VK_SAMPLE_COUNT_1_BIT;
		im.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		im.tiling = VK_IMAGE_TILING_OPTIMAL;
		im.usage =  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		assert(vkCreateImage(m_base.device, &im, m_vk_alloc, &w->tex.image) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, w->tex.image, &mr);
		w->tex.offset = m_device_memory_res.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, w->tex.image, m_device_memory_res.handle, 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = w->tex.image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 2;
		view.subresourceRange.levelCount = 1;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &w->tex.view) == VK_SUCCESS);
	}


	uint64_t params_staging_buffer_offset = m_mappable_memory.allocate(sizeof(resource<RES_TYPE_WATER>::fft_params_data), 1);
	
	water::fft_params_data* params = reinterpret_cast<water::fft_params_data*>(m_mappable_memory.map(params_staging_buffer_offset, 
		sizeof(resource<RES_TYPE_WATER>::fft_params_data)));
	params->base_frequency = build->base_frequency;
	params->sqrtA = sqrtf(build->A);
	params->two_pi_per_L = glm::vec2(2.f*PI) / build->grid_size_in_meters;
	m_mappable_memory.unmap();

	//create fft params buffer
	{
		VkBufferCreateInfo b = {};
		b.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		b.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		b.size = sizeof(water::fft_params_data);
		b.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		assert(vkCreateBuffer(m_base.device, &b, m_vk_alloc, &w->fft_params_buffer) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, w->fft_params_buffer, &mr);
		w->fft_params_offset = m_device_memory_res.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, w->fft_params_buffer, m_device_memory_res.handle(), 0);
	}

	//transition layouts and copy from staging buffers
	{
		begin_build_cb();
		
		VkBufferCopy bc;
		bc.dstOffset = 0;
		bc.size = sizeof(resource<RES_TYPE_WATER>::fft_params_data);
		bc.srcOffset = params_staging_buffer_offset;

		vkCmdCopyBuffer(m_build_cb, m_staging_buffer, w->fft_params_buffer, 1, &bc);

		std::array<VkImageMemoryBarrier, 3> barriers= {};
		for (uint32_t i = 0; i < 3; ++i)
		{
			auto& b = barriers[i];
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.image = i >= 2 ? w->tex.image : w->noise.image;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = i >= 2 ? 2 : 1;	
			b.subresourceRange.levelCount = 1;
		}

		barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barriers[0].srcAccessMask = 0;
		barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		
		barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		barriers[2].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barriers[2].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[2].srcAccessMask = 0;
		barriers[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;


		vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, barriers.data());

		VkBufferImageCopy bic = {};
		bic.bufferImageHeight = 0;
		bic.bufferRowLength = 0;
		bic.bufferOffset = noise_staging_buffer_offset;
		bic.imageOffset = { 0, 0, 0 };
		bic.imageExtent = { resource<RES_TYPE_WATER>::GRID_SIZE, resource<RES_TYPE_WATER>::GRID_SIZE, 1 };
		bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bic.imageSubresource.baseArrayLayer = 0;
		bic.imageSubresource.layerCount = 1;
		bic.imageSubresource.mipLevel = 0;

		vkCmdCopyBufferToImage(m_build_cb, m_staging_buffer, w->noise.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);

		vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, barriers.data()+1);

		vkCmdPipelineBarrier(m_build_cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, barriers.data()+2);

		end_build_cb();
	}

	//allocate dss
	{
		VkDescriptorSet dss[2];

		VkDescriptorSetLayout dsls[2] =
		{
			*m_dsls[DSL_TYPE_WATER],
			*m_dsls[DSL_TYPE_WATER_COMPUTE]
		};

		VkDescriptorSetAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pSetLayouts = dsls;
		alloc.descriptorSetCount = 2;
		alloc.descriptorPool = m_dp_pools[DSL_TYPE_WATER]->use_dp(w->dp_index);

		assert(vkAllocateDescriptorSets(m_base.device, &alloc, dss) == VK_SUCCESS);

		w->ds = dss[0];
		w->fft_ds = dss[1];
	}

	//update dss
	{
		VkDescriptorBufferInfo fft_params;
		fft_params.buffer = w->fft_params_buffer;
		fft_params.offset = 0;
		fft_params.range = sizeof(resource<RES_TYPE_WATER>::fft_params_data);

		VkDescriptorImageInfo noise;
		noise.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		noise.imageView = w->noise.view;

		VkDescriptorImageInfo tex_comp;
		tex_comp.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		tex_comp.imageView = w->tex.view;

		VkDescriptorImageInfo tex_draw;
		tex_draw.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		tex_draw.imageView = w->tex.view;
		tex_draw.sampler = m_samplers[SAMPLER_TYPE_REPEATE];

		VkWriteDescriptorSet writes[4] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].dstArrayElement = 0;
		writes[0].dstBinding = 0;
		writes[0].dstSet = w->fft_ds;
		writes[0].pBufferInfo = &fft_params;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[1].dstArrayElement = 0;
		writes[1].dstBinding = 1;
		writes[1].dstSet = w->fft_ds;
		writes[1].pImageInfo = &noise;

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[2].dstArrayElement = 0;
		writes[2].dstBinding = 2;
		writes[2].dstSet = w->fft_ds;
		writes[2].pImageInfo = &tex_comp;
		
		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[3].dstArrayElement = 0;
		writes[3].dstBinding = 0;
		writes[3].dstSet = w->ds;
		writes[3].pImageInfo = &tex_draw;

		vkUpdateDescriptorSets(m_base.device, 4, writes, 0, nullptr);
	}

	wait_for_build_fence();

	m_mappable_memory.deallocate(noise_staging_buffer_offset);
	m_mappable_memory.deallocate(params_staging_buffer_offset);

	res->ready_bit = true;
}

template<>
void resource_manager::destroy<RES_TYPE_WATER>(base_resource* res)
{
	auto w = reinterpret_cast<resource<RES_TYPE_WATER>*>(res->data);

	VkDescriptorSet dss[2] = { w->ds, w->fft_ds };
	vkFreeDescriptorSets(m_base.device, m_dp_pools[DSL_TYPE_WATER]->stop_using_dp(w->dp_index), 2, dss);

	vkDestroyBuffer(m_base.device, w->fft_params_buffer, m_vk_alloc);
	m_device_memory_res.deallocate(w->fft_params_offset);

	vkDestroyImageView(m_base.device, w->noise.view, m_vk_alloc);
	vkDestroyImageView(m_base.device, w->tex.view, m_vk_alloc);
	vkDestroyImage(m_base.device, w->noise.image, m_vk_alloc);
	vkDestroyImage(m_base.device, w->tex.image, m_vk_alloc);
	m_device_memory_res.deallocate(w->noise.offset);
	m_device_memory_res.deallocate(w->tex.offset);
}