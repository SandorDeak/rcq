#include "resource_manager.h"

#include "tinyobjloader.h"

using namespace rcq;

resource_manager* resource_manager::m_instance = nullptr;

void load_mesh(const std::string& file_name, std::vector<vertex>& vertices, std::vector<uint32_t>& indices,
	bool make_vertex_ext, std::vector<vertex_ext>& vertices_ext);

resource_manager::resource_manager(const base_info& info) : m_base(info)
{
	create_memory_resources_and_containers();
	//create_samplers();
	create_dsls();
	create_dp_pools();
	create_command_pool();

	m_should_end_build = false;
	m_should_end_destroy = false;
	m_build_thread = std::thread([this]()
	{
		build_loop();
	});

	m_destroy_thread = std::thread([this]()
	{
		destroy_loop();
	});
}


resource_manager::~resource_manager()
{
	m_should_end_build.store(true);
	m_build_thread.join();

	m_should_end_destroy.store(true);
	m_destroy_thread.join();

	vkDestroyCommandPool(m_base.device, m_build_cp, m_vk_alloc);
	vkDestroyFence(m_base.device, m_build_f, m_vk_alloc);
	vkDestroyBuffer(m_base.device, m_staging_buffer, m_vk_alloc);
	for (auto& dsl : m_dsls)
		vkDestroyDescriptorSetLayout(m_base.device, dsl, m_vk_alloc);

	for(auto& dp : m_dp_pools)
		dp.reset();

	m_build_queue.reset();
	m_destroy_queue.reset();
	


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





void resource_manager::init(const base_info& info)
{

	assert(m_instance == nullptr);
	m_instance = new resource_manager(info);
}

void resource_manager::destroy()
{
	assert(m_instance != nullptr);
	delete m_instance;
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
	assert(vkQueueSubmit(m_base.queues[QUEUE_RESOURCE_BUILD], 1, &submit, m_build_f) == VK_SUCCESS);
}

void resource_manager::wait_for_build_fence()
{
	assert(vkWaitForFences(m_base.device, 1, &m_build_f, VK_TRUE, ~0) == VK_SUCCESS);
	vkResetFences(m_base.device, 1, &m_build_f);
}





/*void load_mesh(const std::string& file_name, std::vector<vertex>& vertices, std::vector<uint32_t>& indices,
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
}*/

void resource_manager::create_command_pool()
{
	VkCommandPoolCreateInfo cp = {};
	cp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cp.queueFamilyIndex = m_base.queue_families.graphics_family;
	cp.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	assert(vkCreateCommandPool(m_base.device, &cp, m_vk_alloc, &m_build_cp) == VK_SUCCESS);
}



