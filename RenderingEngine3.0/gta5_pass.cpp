#include "gta5_pass.h"

#include "resource_manager.h"
#include "gta5_foundation.h"

using namespace rcq;

gta5_pass* gta5_pass::m_instance = nullptr;

gta5_pass::gta5_pass(const base_info& info, const renderable_container& rends) : m_base(info), m_renderables(rends)
{
	send_memory_requirements();
	create_render_passes();
	create_dsls_and_allocate_dss();
	create_graphics_pipelines();
	create_command_pool();
	create_samplers();
	get_memory_and_build_resources();
	update_descriptor_sets();
	create_framebuffers();
	allocate_command_buffers();
	create_sync_objects();
	record_present_command_buffers();
}


gta5_pass::~gta5_pass()
{
	wait_for_finish();

	vkDestroyFence(m_base.device, m_render_finished_f, host_memory_manager);
	vkDestroySemaphore(m_base.device, m_render_finished_s, host_memory_manager);
	vkDestroySemaphore(m_base.device, m_image_available_s, host_memory_manager);
	for (auto s : m_present_ready_ss)
		vkDestroySemaphore(m_base.device, s, host_memory_manager);

	vkDestroyFramebuffer(m_base.device, m_fbs.environment_map_gen_fb, host_memory_manager);
	vkDestroyFramebuffer(m_base.device, m_fbs.dir_shadow_map_gen_fb, host_memory_manager);
	vkDestroyFramebuffer(m_base.device, m_fbs.frame_image_gen, host_memory_manager);
	for(auto fb : m_fbs.postprocessing)
		vkDestroyFramebuffer(m_base.device, fb, host_memory_manager);

	vkDestroyCommandPool(m_base.device, m_cp, host_memory_manager);

	for (auto s : m_samplers)
		vkDestroySampler(m_base.device, s, host_memory_manager);

	for (auto& im : m_res_image)
	{
		vkDestroyImageView(m_base.device, im.view, host_memory_manager);
		vkDestroyImage(m_base.device, im.image, host_memory_manager);
	}

	vkUnmapMemory(m_base.device, m_res_data.staging_buffer_mem);
	vkDestroyBuffer(m_base.device, m_res_data.staging_buffer, host_memory_manager);
	vkDestroyBuffer(m_base.device, m_res_data.buffer, host_memory_manager);

	vkDestroyDescriptorPool(m_base.device, m_dp, host_memory_manager);

	auto package = std::make_unique<destroy_package>();
	package->ids[RESOURCE_TYPE_MEMORY].push_back(RENDER_ENGINE_GTA5);
	resource_manager::instance()->push_destroy_package(std::move(package));

	for (auto& gp : m_gps)
	{
		vkDestroyPipeline(m_base.device, gp.gp, host_memory_manager);
		vkDestroyPipelineLayout(m_base.device, gp.pl, host_memory_manager);
		vkDestroyDescriptorSetLayout(m_base.device, gp.dsl, host_memory_manager);
	}
	for(auto pass : m_passes)
		vkDestroyRenderPass(m_base.device, pass, host_memory_manager);
}

void gta5_pass::init(const base_info& info, const renderable_container& rends)
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("gta5_pass is already initialised!");
	}
	m_instance = new gta5_pass(info, rends);
}

void gta5_pass::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy gta5_pass, it doesn't exist!");
	}

	delete m_instance;
}

void gta5_pass::create_render_passes()
{
	if (vkCreateRenderPass(m_base.device, &render_pass_environment_map_gen::create_info,
		host_memory_manager, &m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN]) != VK_SUCCESS)
		throw std::runtime_error("failed to create environment map gen render pass!");


	if (vkCreateRenderPass(m_base.device, &render_pass_dir_shadow_map_gen::create_info,
		host_memory_manager, &m_passes[RENDER_PASS_DIR_SHADOW_MAP_GEN]) != VK_SUCCESS)
		throw std::runtime_error("failed to create dir shadow map render pass!");


	if (vkCreateRenderPass(m_base.device, &render_pass_frame_image_gen::create_info,
		host_memory_manager, &m_passes[RENDER_PASS_FRAME_IMAGE_GEN]) != VK_SUCCESS)
		throw std::runtime_error("failed to create frame image gen render pass!");

	if (vkCreateRenderPass(m_base.device, &render_pass_postprocessing::create_info,
		host_memory_manager, &m_passes[RENDER_PASS_POSTPROCESSING]) != VK_SUCCESS)
		throw std::runtime_error("failed to create postprocessing render pass!");
}

void gta5_pass::create_graphics_pipelines()
{
	std::array<VkGraphicsPipelineCreateInfo, GP_COUNT> create_infos = {};

	render_pass_environment_map_gen::subpass_unique::pipelines::mat::runtime_info r0(m_base.device);
	r0.fill_create_info(create_infos[GP_ENVIRONMENT_MAP_GEN_MAT]);
	create_infos[GP_ENVIRONMENT_MAP_GEN_MAT].renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];

	render_pass_environment_map_gen::subpass_unique::pipelines::skybox::runtime_info r1(m_base.device);
	r1.fill_create_info(create_infos[GP_ENVIRONMENT_MAP_GEN_SKYBOX]);
	create_infos[GP_ENVIRONMENT_MAP_GEN_SKYBOX].renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];

	render_pass_dir_shadow_map_gen::subpass_unique::pipeline::runtime_info r2(m_base.device);
	r2.fill_create_info(create_infos[GP_DIR_SHADOW_MAP_GEN]);
	create_infos[GP_DIR_SHADOW_MAP_GEN].renderPass = m_passes[RENDER_PASS_DIR_SHADOW_MAP_GEN];

	render_pass_frame_image_gen::subpass_gbuffer_gen::pipeline::runtime_info r3(m_base.device, m_base.swap_chain_image_extent);
	r3.fill_create_info(create_infos[GP_GBUFFER_GEN]);
	create_infos[GP_GBUFFER_GEN].renderPass = m_passes[RENDER_PASS_FRAME_IMAGE_GEN];

	render_pass_frame_image_gen::subpass_ss_dir_shadow_map_gen::pipeline::runtime_info r4(m_base.device, m_base.swap_chain_image_extent);
	r4.fill_create_info(create_infos[GP_SS_DIR_SHADOW_MAP_GEN]);
	create_infos[GP_SS_DIR_SHADOW_MAP_GEN].renderPass = m_passes[RENDER_PASS_FRAME_IMAGE_GEN];

	render_pass_frame_image_gen::subpass_ss_dir_shadow_map_blur::pipeline::runtime_info r5(m_base.device, m_base.swap_chain_image_extent);
	r5.fill_create_info(create_infos[GP_SS_DIR_SHADOW_MAP_BLUR]);
	create_infos[GP_SS_DIR_SHADOW_MAP_BLUR].renderPass = m_passes[RENDER_PASS_FRAME_IMAGE_GEN];

	render_pass_frame_image_gen::subpass_ssao_map_gen::pipeline::runtime_info r6(m_base.device, m_base.swap_chain_image_extent);
	r6.fill_create_info(create_infos[GP_SSAO_GEN]);
	create_infos[GP_SSAO_GEN].renderPass = m_passes[RENDER_PASS_FRAME_IMAGE_GEN];

	render_pass_frame_image_gen::subpass_ssao_map_blur::pipeline::runtime_info r7(m_base.device, m_base.swap_chain_image_extent);
	r7.fill_create_info(create_infos[GP_SSAO_BLUR]);
	create_infos[GP_SSAO_BLUR].renderPass = m_passes[RENDER_PASS_FRAME_IMAGE_GEN];

	render_pass_frame_image_gen::subpass_image_assembler::pipeline::runtime_info r8(m_base.device, m_base.swap_chain_image_extent);
	r8.fill_create_info(create_infos[GP_IMAGE_ASSEMBLER]);
	create_infos[GP_IMAGE_ASSEMBLER].renderPass = m_passes[RENDER_PASS_FRAME_IMAGE_GEN];

	render_pass_postprocessing::subpass_bypass::pipeline::runtime_info r9(m_base.device, m_base.swap_chain_image_extent);
	r9.fill_create_info(create_infos[GP_POSTPROCESSING]);
	create_infos[GP_POSTPROCESSING].renderPass = m_passes[RENDER_PASS_POSTPROCESSING];

	//create layouts
	m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].create_layout(m_base.device, 
	{
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_TR),
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_MAT_EM)
	});

	m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].create_layout(m_base.device, 
	{ 
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX) 
	});

	m_gps[GP_DIR_SHADOW_MAP_GEN].create_layout(m_base.device, 
	{
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_TR)
	});

	m_gps[GP_GBUFFER_GEN].create_layout(m_base.device, 
	{
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_TR),
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE)
	});

	m_gps[GP_SS_DIR_SHADOW_MAP_GEN].create_layout(m_base.device, {});
	m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].create_layout(m_base.device, {});
	m_gps[GP_SSAO_GEN].create_layout(m_base.device, {});
	m_gps[GP_SSAO_BLUR].create_layout(m_base.device, {});
	m_gps[GP_IMAGE_ASSEMBLER].create_layout(m_base.device, {});
	m_gps[GP_POSTPROCESSING].create_layout(m_base.device, {});

	for (uint32_t i = 0; i < GP_COUNT; ++i)
		create_infos[i].layout = m_gps[i].pl;


	//create graphics pipelines 
	std::array<VkPipeline, GP_COUNT> gps;
	for (uint32_t i = 0; i < GP_COUNT; ++i)
	{
		vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, 1, &create_infos[i], host_memory_manager, &gps[i]);
		int k = i;
	}
	/*if (vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, GP_COUNT, create_infos.data(), host_memory_manager, gps.data())
		!= VK_SUCCESS)
		throw std::runtime_error("failed to create gta5 graphics pipelines!");*/

	for (uint32_t i = 0; i < GP_COUNT; ++i)
		m_gps[i].gp = gps[i];
}

void gta5_pass::create_dsls_and_allocate_dss()
{
	//create dsls

	m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].create_dsl(m_base.device,
		render_pass_environment_map_gen::subpass_unique::pipelines::mat::dsl::create_info);

	m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].create_dsl(m_base.device,
		render_pass_environment_map_gen::subpass_unique::pipelines::skybox::dsl::create_info);

	m_gps[GP_DIR_SHADOW_MAP_GEN].create_dsl(m_base.device,
		render_pass_dir_shadow_map_gen::subpass_unique::pipeline::dsl::create_info);


	m_gps[GP_GBUFFER_GEN].create_dsl(m_base.device,
		render_pass_frame_image_gen::subpass_gbuffer_gen::pipeline::dsl::create_info);

	m_gps[GP_SS_DIR_SHADOW_MAP_GEN].create_dsl(m_base.device,
		render_pass_frame_image_gen::subpass_ss_dir_shadow_map_gen::pipeline::dsl::create_info);


	m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].create_dsl(m_base.device,
		render_pass_frame_image_gen::subpass_ss_dir_shadow_map_blur::pipeline::dsl::create_info);

	m_gps[GP_SSAO_GEN].create_dsl(m_base.device,
		render_pass_frame_image_gen::subpass_ssao_map_gen::pipeline::dsl::create_info);

	m_gps[GP_SSAO_BLUR].create_dsl(m_base.device,
		render_pass_frame_image_gen::subpass_ssao_map_blur::pipeline::dsl::create_info);

	m_gps[GP_IMAGE_ASSEMBLER].create_dsl(m_base.device,
		render_pass_frame_image_gen::subpass_image_assembler::pipeline::dsl::create_info);

	m_gps[GP_POSTPROCESSING].create_dsl(m_base.device,
		render_pass_postprocessing::subpass_bypass::pipeline::dsl::create_info);

	//create descriptor pool
	if (vkCreateDescriptorPool(m_base.device, &dp::create_info,
		host_memory_manager, &m_dp) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor pool!");

	//allocate dss
	std::array<VkDescriptorSet, GP_COUNT> dss;
	std::array<VkDescriptorSetLayout, GP_COUNT> dsls;

	VkDescriptorSetAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = m_dp;
	alloc.descriptorSetCount = GP_COUNT;
	alloc.pSetLayouts = dsls.data();

	for (uint32_t i = 0; i < GP_COUNT; ++i)
		dsls[i] = m_gps[i].dsl;

	if (vkAllocateDescriptorSets(m_base.device, &alloc, dss.data()) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate descriptor sets!");

	for (uint32_t i = 0; i < GP_COUNT; ++i)
		m_gps[i].ds = dss[i];
}

void gta5_pass::send_memory_requirements()
{
	std::vector<VkMemoryAllocateInfo> alloc_infos(MEMORY_COUNT);
	for (auto& alloc : alloc_infos)
		alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;


	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(m_base.physical_device, &props);
	size_t ub_alignment = static_cast<size_t>(props.limits.minUniformBufferOffsetAlignment);
	m_res_data.calcoffset_and_size(ub_alignment);

	size_t size = m_res_data.size;

	//staging buffer
	{
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer.size = size;
		buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		if (vkCreateBuffer(m_base.device, &buffer, host_memory_manager, &m_res_data.staging_buffer) != VK_SUCCESS)
			throw std::runtime_error("failed to create buffer!");
		
		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, m_res_data.staging_buffer, &mr);

		alloc_infos[MEMORY_STAGING_BUFFER].allocationSize = mr.size;
		alloc_infos[MEMORY_STAGING_BUFFER].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}

	//buffer
	{
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer.size = size;
		buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		if (vkCreateBuffer(m_base.device, &buffer, host_memory_manager, &m_res_data.buffer) != VK_SUCCESS)
			throw std::runtime_error("failed to create buffer!");

		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, m_res_data.buffer, &mr);

		alloc_infos[MEMORY_BUFFER].allocationSize = mr.size;
		alloc_infos[MEMORY_BUFFER].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//environment map depthstencil
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		image.arrayLayers = 6;
		image.extent.depth = 1;
		image.extent.height = ENVIRONMENT_MAP_SIZE;
		image.extent.width = ENVIRONMENT_MAP_SIZE;
		image.format = VK_FORMAT_D32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image, &mr);

		alloc_infos[MEMORY_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].allocationSize = mr.size;
		alloc_infos[MEMORY_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//environment map
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		image.arrayLayers = 6;
		image.extent.depth = 1;
		image.extent.height = ENVIRONMENT_MAP_SIZE;
		image.extent.width = ENVIRONMENT_MAP_SIZE;
		image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image, &mr);

		alloc_infos[MEMORY_ENVIRONMENT_MAP].allocationSize = mr.size;
		alloc_infos[MEMORY_ENVIRONMENT_MAP].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//gbuffer pos roughness
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.depth = 1;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image, &mr);

		alloc_infos[MEMORY_GB_POS_ROUGHNESS].allocationSize = mr.size;
		alloc_infos[MEMORY_GB_POS_ROUGHNESS].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//gbuffer F0 ssao
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.depth = 1;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_GB_F0_SSAO].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_F0_SSAO].image, &mr);

		alloc_infos[MEMORY_GB_F0_SSAO].allocationSize = mr.size;
		alloc_infos[MEMORY_GB_F0_SSAO].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//gbuffer albedo ssds
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.depth = 1;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].image, &mr);

		alloc_infos[MEMORY_GB_ALBEDO_SSDS].allocationSize = mr.size;
		alloc_infos[MEMORY_GB_ALBEDO_SSDS].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//gbuffer normal ao
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.depth = 1;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_GB_NORMAL_AO].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_NORMAL_AO].image, &mr);

		alloc_infos[MEMORY_GB_NORMAL_AO].allocationSize = mr.size;
		alloc_infos[MEMORY_GB_NORMAL_AO].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//preimage
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.depth = 1;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_PREIMAGE].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_PREIMAGE].image, &mr);

		alloc_infos[MEMORY_PREIMAGE].allocationSize = mr.size;
		alloc_infos[MEMORY_PREIMAGE].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//gbuffer depth
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.depth = 1;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage =  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_GB_DEPTH].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_DEPTH].image, &mr);

		alloc_infos[MEMORY_GB_DEPTH].allocationSize = mr.size;
		alloc_infos[MEMORY_GB_DEPTH].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//dir shadow map
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = FRUSTUM_SPLIT_COUNT;
		image.extent.depth = 1;
		image.extent.height = DIR_SHADOW_MAP_SIZE;
		image.extent.width = DIR_SHADOW_MAP_SIZE;
		image.format = VK_FORMAT_D32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image, &mr);

		alloc_infos[MEMORY_DIR_SHADOW_MAP].allocationSize = mr.size;
		alloc_infos[MEMORY_DIR_SHADOW_MAP].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//ss dir shadow map
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.depth = 1;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.format = VK_FORMAT_D32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image, &mr);

		alloc_infos[MEMORY_SS_DIR_SHADOW_MAP].allocationSize = mr.size;
		alloc_infos[MEMORY_SS_DIR_SHADOW_MAP].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	//ssao map
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.depth = 1;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.format = VK_FORMAT_D32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		if (vkCreateImage(m_base.device, &image, host_memory_manager, &m_res_image[RES_IMAGE_SSAO_MAP].image)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_SSAO_MAP].image, &mr);

		alloc_infos[MEMORY_SSAO_MAP].allocationSize = mr.size;
		alloc_infos[MEMORY_SSAO_MAP].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	build_package package;
	std::get<RESOURCE_TYPE_MEMORY>(package).emplace_back(RENDER_ENGINE_GTA5, std::move(alloc_infos));

	resource_manager::instance()->process_build_package(std::move(package));
}

void gta5_pass::get_memory_and_build_resources()
{
	memory mem = resource_manager::instance()->get_res<RESOURCE_TYPE_MEMORY>(RENDER_ENGINE_GTA5);

	//staging buffer
	{
		m_res_data.staging_buffer_mem = mem[MEMORY_STAGING_BUFFER];
		vkBindBufferMemory(m_base.device, m_res_data.staging_buffer, m_res_data.staging_buffer_mem, 0);

		void* raw_data;
		vkMapMemory(m_base.device, m_res_data.staging_buffer_mem, 0, m_res_data.size, 0, &raw_data);
		char* data = static_cast<char*>(raw_data);
		m_res_data.set_pointers(data, std::make_index_sequence<RES_DATA_COUNT>());
	}

	//buffer
	{
		vkBindBufferMemory(m_base.device, m_res_data.buffer, mem[MEMORY_BUFFER], 0);
	}

	//environment map gen depthstencil
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image,
			mem[MEMORY_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_D32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 6;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}

	//environment map 
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image,
			mem[MEMORY_ENVIRONMENT_MAP], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 6;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}

	//gbuffer pos roughness
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image,
			mem[MEMORY_GB_POS_ROUGHNESS], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}

	//gbuffer F0 ssao
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_F0_SSAO].image,
			mem[MEMORY_GB_F0_SSAO], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_GB_F0_SSAO].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_GB_F0_SSAO].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}

	//gbuffer albedo ssds
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].image,
			mem[MEMORY_GB_ALBEDO_SSDS], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}

	//gbuffer normal ao
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_NORMAL_AO].image,
			mem[MEMORY_GB_NORMAL_AO], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_GB_NORMAL_AO].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_GB_NORMAL_AO].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}

	//preimage
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_PREIMAGE].image,
			mem[MEMORY_PREIMAGE], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_PREIMAGE].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_PREIMAGE].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}
	//gbuffer depth
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_DEPTH].image,
			mem[MEMORY_GB_DEPTH], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
		view.image = m_res_image[RES_IMAGE_GB_DEPTH].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_GB_DEPTH].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}

	//dir shadow map
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image,
			mem[MEMORY_DIR_SHADOW_MAP], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_D32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = FRUSTUM_SPLIT_COUNT;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_DIR_SHADOW_MAP].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}

	//ss dir shadow map
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image,
			mem[MEMORY_SS_DIR_SHADOW_MAP], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_D32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}

	//ssao map
	{
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_SSAO_MAP].image,
			mem[MEMORY_SSAO_MAP], 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_D32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_SSAO_MAP].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &view, host_memory_manager, &m_res_image[RES_IMAGE_SSAO_MAP].view)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create image view!");
	}

	/*//transition images to proper layout
	VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp);
	std::array<VkImageMemoryBarrier, RES_IMAGE_COUNT> barriers = {};

	//environment map depthstencil
	{
		auto& b = barriers[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL];
		b.image = m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 6;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;
	}

	//environment map
	{
		auto& b = barriers[RES_IMAGE_ENVIRONMENT_MAP];
		b.image = m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 6;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;
	}

	//gbuffer pos roughness
	{
		auto& b = barriers[RES_IMAGE_GB_POS_ROUGHNESS];
		b.image = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 1;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;
	}

	//gbuffer F0 ssao
	{
		auto& b = barriers[RES_IMAGE_GB_F0_SSAO];
		b.image = m_res_image[RES_IMAGE_GB_F0_SSAO].image;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 1;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;
	}

	//gbuffer albedo ssds
	{
		auto& b = barriers[RES_IMAGE_GB_ALBEDO_SSDS];
		b.image = m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].image;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 1;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;
	}

	//gbuffer normal ao
	{
		auto& b = barriers[RES_IMAGE_GB_NORMAL_AO];
		b.image = m_res_image[RES_IMAGE_GB_NORMAL_AO].image;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 1;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;
	}

	//gbuffer depth
	{
		auto& b = barriers[RES_IMAGE_GB_DEPTH];
		b.image = m_res_image[RES_IMAGE_GB_DEPTH].image;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 1;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;
	}

	//dir shadow map
	{
		auto& b = barriers[RES_IMAGE_DIR_SHADOW_MAP];
		b.image = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 1;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;
	}

	//ss dir shadow map
	{
		auto& b = barriers[RES_IMAGE_SS_DIR_SHADOW_MAP];
		b.image = m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 1;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;
	}*/
}

void gta5_pass::create_command_pool()
{
	VkCommandPoolCreateInfo pool = {};
	pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool.queueFamilyIndex = m_base.queue_families.graphics_family;

	if (vkCreateCommandPool(m_base.device, &pool, host_memory_manager, &m_cp) != VK_SUCCESS)
		throw std::runtime_error("failed to create command pool!");
}

void gta5_pass::update_descriptor_sets()
{
	std::vector<VkWriteDescriptorSet> w;
	
	//environment map gen mat
	{
		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_ENVIRONMENT_MAP_GEN_MAT_DATA];
		ub.range = sizeof(environment_map_gen_mat_data);

		w.resize(1);
		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].ds;
		w[0].pBufferInfo = &ub;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//environment map gen skybox
	{
		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX_DATA];
		ub.range = sizeof(environment_map_gen_skybox_data);

		w.resize(1);
		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].ds;
		w[0].pBufferInfo = &ub;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//gbuffer gen
	{
		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_GBUFFER_GEN_DATA];
		ub.range = sizeof(gbuffer_gen_data);

		w.resize(1);
		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_GBUFFER_GEN].ds;
		w[0].pBufferInfo = &ub;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//dir shadow map gen
	{

		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_DIR_SHADOW_MAP_GEN_DATA];
		ub.range = sizeof(dir_shadow_map_gen_data);

		w.resize(1);
		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_DIR_SHADOW_MAP_GEN].ds;
		w[0].pBufferInfo = &ub;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//ss dir shadow map gen
	{
		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_SS_DIR_SHADOW_MAP_GEN_DATA];
		ub.range = sizeof(ss_dir_shadow_map_gen_data);

		VkDescriptorImageInfo shadow_tex = {};
		shadow_tex.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadow_tex.imageView = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].view;
		shadow_tex.sampler = m_samplers[SAMPLER_GENERAL];
		
		VkDescriptorImageInfo pos = {};
		pos.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;

		w.resize(3);
		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_GEN].ds;
		w[0].pBufferInfo = &ub;

		w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[1].descriptorCount = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstArrayElement = 0;
		w[1].dstBinding = 1;
		w[1].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_GEN].ds;
		w[1].pImageInfo = &shadow_tex;

		w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[2].descriptorCount = 1;
		w[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[2].dstArrayElement = 0;
		w[2].dstBinding = 2;
		w[2].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_GEN].ds;
		w[2].pImageInfo = &pos;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//ss dir shadow map blur
	{
		VkDescriptorImageInfo pos;
		pos.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		pos.sampler=m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo shadow;
		shadow.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadow.imageView = m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].view;
		shadow.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo normal;
		normal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normal.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;

		w.resize(3);
		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].ds;
		w[0].pImageInfo = &pos;

		w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[1].descriptorCount = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstArrayElement = 0;
		w[1].dstBinding = 1;
		w[1].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].ds;
		w[1].pImageInfo = &shadow;

		w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[2].descriptorCount = 1;
		w[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[2].dstArrayElement = 0;
		w[2].dstBinding = 2;
		w[2].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].ds;
		w[2].pImageInfo = &normal;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//ssao gen
	{
		VkDescriptorImageInfo pos;
		pos.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		pos.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo normal;
		normal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normal.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;

		w.resize(2);
		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_SSAO_GEN].ds;
		w[0].pImageInfo = &pos;

		w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[1].descriptorCount = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[1].dstArrayElement = 0;
		w[1].dstBinding = 1;
		w[1].dstSet = m_gps[GP_SSAO_GEN].ds;
		w[1].pImageInfo = &normal;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//ssao blur
	{
		VkDescriptorImageInfo ssao;
		ssao.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		ssao.imageView = m_res_image[RES_IMAGE_SSAO_MAP].view;
		ssao.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		w.resize(1);
		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_SSAO_BLUR].ds;
		w[0].pImageInfo = &ssao;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}
	
	//image assembler
	{
		VkDescriptorImageInfo gb_pos_roughness;
		gb_pos_roughness.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_pos_roughness.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;

		VkDescriptorImageInfo gb_F0_ssao;
		gb_F0_ssao.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_F0_ssao.imageView = m_res_image[RES_IMAGE_GB_F0_SSAO].view;

		VkDescriptorImageInfo gb_albedo_ssds;
		gb_albedo_ssds.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_albedo_ssds.imageView = m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].view;
		
		VkDescriptorImageInfo gb_normal_ao;
		gb_normal_ao.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_normal_ao.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;

		VkDescriptorImageInfo em;
		em.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		em.imageView = m_res_image[RES_IMAGE_ENVIRONMENT_MAP].view;
		em.sampler = m_samplers[SAMPLER_GENERAL];

		VkDescriptorBufferInfo ub;
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_IMAGE_ASSEMBLER_DATA];
		ub.range = sizeof(image_assembler_data);

		w.resize(6);

		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[0].pImageInfo = &gb_pos_roughness;

		w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[1].descriptorCount = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[1].dstArrayElement = 0;
		w[1].dstBinding = 1;
		w[1].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[1].pImageInfo = &gb_F0_ssao;

		w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[2].descriptorCount = 1;
		w[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[2].dstArrayElement = 0;
		w[2].dstBinding = 2;
		w[2].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[2].pImageInfo = &gb_albedo_ssds;

		w[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[3].descriptorCount = 1;
		w[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[3].dstArrayElement = 0;
		w[3].dstBinding = 3;
		w[3].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[3].pImageInfo = &gb_normal_ao;

		w[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[4].descriptorCount = 1;
		w[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[4].dstArrayElement = 0;
		w[4].dstBinding = 4;
		w[4].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[4].pImageInfo = &em;

		w[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[5].descriptorCount = 1;
		w[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[5].dstArrayElement = 0;
		w[5].dstBinding = 5;
		w[5].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[5].pBufferInfo = &ub;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//postprocessing
	{
		VkDescriptorImageInfo preimage = {};
		preimage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		preimage.imageView = m_res_image[RES_IMAGE_PREIMAGE].view;
		preimage.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		w.resize(1);

		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_POSTPROCESSING].ds;
		w[0].pImageInfo = &preimage;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}
}

void gta5_pass::create_samplers()
{
	//general
	{
		VkSamplerCreateInfo sampler = {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.anisotropyEnable = VK_TRUE;
		sampler.compareEnable = VK_FALSE;
		sampler.unnormalizedCoordinates = VK_FALSE;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.maxAnisotropy = 16.f;
		sampler.minLod = 0.f;
		sampler.maxLod = 0.f;
		sampler.mipLodBias = 0.f;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		if (vkCreateSampler(m_base.device, &sampler, host_memory_manager, &m_samplers[SAMPLER_GENERAL]) != VK_SUCCESS)
			throw std::runtime_error("failed to create sampler!");
	}

	//unnormalized coord
	{
		VkSamplerCreateInfo sampler = {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.anisotropyEnable = VK_FALSE;
		sampler.compareEnable = VK_FALSE;
		sampler.unnormalizedCoordinates = VK_TRUE;
		sampler.magFilter = VK_FILTER_NEAREST;
		sampler.minFilter = VK_FILTER_NEAREST;
		sampler.minLod = 0.f;
		sampler.maxLod = 0.f;
		sampler.mipLodBias = 0.f;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		if (vkCreateSampler(m_base.device, &sampler, host_memory_manager, &m_samplers[SAMPLER_UNNORMALIZED_COORD]) != VK_SUCCESS)
			throw std::runtime_error("failed to create sampler!");
	}
}

void gta5_pass::create_sync_objects()
{
	VkSemaphoreCreateInfo s = {};
	s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if (vkCreateSemaphore(m_base.device, &s, host_memory_manager, &m_image_available_s) != VK_SUCCESS ||
		vkCreateSemaphore(m_base.device, &s, host_memory_manager, &m_render_finished_s) != VK_SUCCESS)
		throw std::runtime_error("failed to create semaphores!");

	m_present_ready_ss.resize(m_base.swap_chain_image_views.size());
	for (auto& sem : m_present_ready_ss)
	{
		if (vkCreateSemaphore(m_base.device, &s, host_memory_manager, &sem) != VK_SUCCESS)
			throw std::runtime_error("failed to create semaphore!");
	}

	VkFenceCreateInfo f = {};
	f.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	f.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	if (vkCreateFence(m_base.device, &f, host_memory_manager, &m_render_finished_f) != VK_SUCCESS)
		throw std::runtime_error("failed to create fence!");
}

void gta5_pass::create_framebuffers()
{
	m_fbs.postprocessing.resize(m_base.swap_chain_image_views.size());

	//environment map gen
	{
		using namespace render_pass_environment_map_gen;

		std::array<VkImageView, ATT_COUNT> atts;
		atts[ATT_COLOR] = m_res_image[RES_IMAGE_ENVIRONMENT_MAP].view;
		atts[ATT_DEPTH] = m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT_COUNT;
		fb.pAttachments = atts.data();
		fb.height = ENVIRONMENT_MAP_SIZE;
		fb.width = ENVIRONMENT_MAP_SIZE;
		fb.layers = 6;
		fb.renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];

		if (vkCreateFramebuffer(m_base.device, &fb, host_memory_manager, &m_fbs.environment_map_gen_fb) != VK_SUCCESS)
			throw std::runtime_error("failed to create framebuffer!");
	}

	//dir shadow map gen
	{
		using namespace render_pass_dir_shadow_map_gen;

		std::array<VkImageView, ATT_COUNT> atts;
		atts[ATT_DEPTH] = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT_COUNT;
		fb.pAttachments = atts.data();
		fb.height = DIR_SHADOW_MAP_SIZE;
		fb.width = DIR_SHADOW_MAP_SIZE;
		fb.layers = FRUSTUM_SPLIT_COUNT;
		fb.renderPass = m_passes[RENDER_PASS_DIR_SHADOW_MAP_GEN];

		if (vkCreateFramebuffer(m_base.device, &fb, host_memory_manager, &m_fbs.dir_shadow_map_gen_fb) != VK_SUCCESS)
			throw std::runtime_error("failed to create framebuffer!");
	}

	//frame image gen
	{
		using namespace render_pass_frame_image_gen;

		std::array<VkImageView, ATT_COUNT> atts;
		atts[ATT_POS_ROUGHNESS] = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		atts[ATT_ALBEDO_SSDS] = m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].view;
		atts[ATT_F0_SSAO] = m_res_image[RES_IMAGE_GB_F0_SSAO].view;
		atts[ATT_NORMAL_AO] = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		atts[ATT_DEPTHSTENCIL] = m_res_image[RES_IMAGE_GB_DEPTH].view;
		atts[ATT_SSAO_MAP] = m_res_image[RES_IMAGE_SSAO_MAP].view;
		atts[ATT_SS_DIR_SHADOW_MAP] = m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].view;
		atts[ATT_PREIMAGE] = m_res_image[RES_IMAGE_PREIMAGE].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT_COUNT;
		fb.pAttachments = atts.data();
		fb.height = m_base.swap_chain_image_extent.height;
		fb.width = m_base.swap_chain_image_extent.width;
		fb.layers = 1;
		fb.renderPass = m_passes[RENDER_PASS_FRAME_IMAGE_GEN];

		if (vkCreateFramebuffer(m_base.device, &fb, host_memory_manager, &m_fbs.frame_image_gen) != VK_SUCCESS)
			throw std::runtime_error("failed to create framebuffer!");
	}

	//postprocessing
	{
		m_fbs.postprocessing.resize(m_base.swap_chain_image_views.size());

		using namespace render_pass_postprocessing;

		std::array<VkImageView, ATT_COUNT> atts;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = atts.size();
		fb.pAttachments = atts.data();
		fb.height = m_base.swap_chain_image_extent.height;
		fb.width = m_base.swap_chain_image_extent.width;
		fb.layers = 1;
		fb.renderPass = m_passes[RENDER_PASS_POSTPROCESSING];

		for (uint32_t i=0; i<m_fbs.postprocessing.size(); ++i)
		{
			atts[ATT_SWAP_CHAIN_IMAGE] = m_base.swap_chain_image_views[i];

			if (vkCreateFramebuffer(m_base.device, &fb, host_memory_manager, &m_fbs.postprocessing[i]) != VK_SUCCESS)
				throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

void gta5_pass::allocate_command_buffers()
{
	//primary cbc
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = 1;
		alloc.commandPool = m_cp;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		
		if (vkAllocateCommandBuffers(m_base.device, &alloc, &m_render_cb) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate primary command buffers!");

		m_present_cbs.resize(m_base.swap_chain_image_views.size());
		for (auto& cb : m_present_cbs)
		{
			if (vkAllocateCommandBuffers(m_base.device, &alloc, &cb) != VK_SUCCESS)
				throw std::runtime_error("failed to allocate primary command buffers!");
		}
	}

	//secondary cbs
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = m_secondary_cbs.size();
		alloc.commandPool = m_cp;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

		if (vkAllocateCommandBuffers(m_base.device, &alloc, m_secondary_cbs.data()) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate primary command buffers!");
	}
}

void gta5_pass::record_present_command_buffers()
{
	for (uint32_t i = 0; i < m_present_cbs.size(); ++i)
	{
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = 0;
		if (vkBeginCommandBuffer(m_present_cbs[i], &begin) != VK_SUCCESS)
			throw std::runtime_error("failed to begin command buffer!");

		VkRenderPassBeginInfo pass = {};
		pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		pass.clearValueCount = 0;
		pass.framebuffer = m_fbs.postprocessing[i];
		pass.renderPass = m_passes[RENDER_PASS_POSTPROCESSING];
		pass.renderArea.extent = m_base.swap_chain_image_extent;
		pass.renderArea.offset = { 0,0 };
		
		vkCmdBeginRenderPass(m_present_cbs[i], &pass, VK_SUBPASS_CONTENTS_INLINE);
		m_gps[GP_POSTPROCESSING].bind(m_present_cbs[i]);
		vkCmdDraw(m_present_cbs[i], 4, 1, 0, 0);
		vkCmdEndRenderPass(m_present_cbs[i]);

		if (vkEndCommandBuffer(m_present_cbs[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to record command buffer!");
	}
}


void gta5_pass::render(const render_settings & settings, std::bitset<RENDERABLE_TYPE_COUNT> record_mask)
{
	process_settings(settings);

	uint32_t image_index;
	vkAcquireNextImageKHR(m_base.device, m_base.swap_chain, std::numeric_limits<uint64_t>::max(), m_image_available_s,
		VK_NULL_HANDLE, &image_index);
	
	vkWaitForFences(m_base.device, 1, &m_render_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_base.device, 1, &m_render_finished_f);

	if (record_mask[RENDERABLE_TYPE_MAT_EM] && !m_renderables[RENDERABLE_TYPE_MAT_EM].empty())
	{
		auto cb = m_secondary_cbs[SECONDARY_CB_MAT_EM];

		VkCommandBufferInheritanceInfo inharitance = {};
		inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inharitance.framebuffer = m_fbs.environment_map_gen_fb;
		inharitance.occlusionQueryEnable = VK_FALSE;
		inharitance.renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];
		inharitance.subpass = 0;

		VkCommandBufferBeginInfo begin = {};
		begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT|VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.pInheritanceInfo = &inharitance;

		if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS)
			throw std::runtime_error("failed to begin command buffer!");

		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].gp);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].pl,
			0, 1, &m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].ds, 0, nullptr);

		for (auto& r : m_renderables[RENDERABLE_TYPE_MAT_EM])
		{
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].pl,
				1, 1, &r.tr_ds, 0, nullptr);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].pl,
				2, 1, &r.mat_light_ds, 0, nullptr);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cb, 0, 1, &r.m.vb, &offset);
			vkCmdBindIndexBuffer(cb, r.m.ib, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cb, r.m.size, 1, 0, 0, 0);
		}

		if(vkEndCommandBuffer(cb)!=VK_SUCCESS)
			throw std::runtime_error("failed to record command buffer!");
	}
	if (record_mask[RENDERABLE_TYPE_SKYBOX] && !m_renderables[RENDERABLE_TYPE_SKYBOX].empty())
	{
		auto cb = m_secondary_cbs[SECONDARY_CB_SKYBOX_EM];

		VkCommandBufferInheritanceInfo inharitance = {};
		inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inharitance.framebuffer = m_fbs.environment_map_gen_fb;
		inharitance.occlusionQueryEnable = VK_FALSE;
		inharitance.renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];
		inharitance.subpass = 0;

		VkCommandBufferBeginInfo begin = {};
		begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.pInheritanceInfo = &inharitance;

		if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS)
			throw std::runtime_error("failed to begin command buffer!");

		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].gp);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].pl,
			0, 1, &m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].ds, 0, nullptr);

		for (auto& r : m_renderables[RENDERABLE_TYPE_SKYBOX])
		{
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].pl,
				1, 1, &r.mat_light_ds, 0, nullptr);
			vkCmdDraw(cb, 1, 1, 0, 0);
		}

		if (vkEndCommandBuffer(cb) != VK_SUCCESS)
			throw std::runtime_error("failed to record command buffer!");
	}

	if (record_mask[RENDERABLE_TYPE_MAT_OPAQUE] && !m_renderables[RENDERABLE_TYPE_MAT_OPAQUE].empty())
	{
		//shadow map gen
		{
			auto cb = m_secondary_cbs[SECONDARY_CB_DIR_SHADOW_GEN];

			VkCommandBufferInheritanceInfo inharitance = {};
			inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inharitance.framebuffer = m_fbs.dir_shadow_map_gen_fb;
			inharitance.occlusionQueryEnable = VK_FALSE;
			inharitance.renderPass = m_passes[RENDER_PASS_DIR_SHADOW_MAP_GEN];
			inharitance.subpass = 0;

			VkCommandBufferBeginInfo begin = {};
			begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin.pInheritanceInfo = &inharitance;

			if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS)
				throw std::runtime_error("failed to begin command buffer!");

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_DIR_SHADOW_MAP_GEN].gp);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_DIR_SHADOW_MAP_GEN].pl,
				0, 1, &m_gps[GP_DIR_SHADOW_MAP_GEN].ds, 0, nullptr);

			for (auto& r : m_renderables[RENDERABLE_TYPE_MAT_OPAQUE])
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_DIR_SHADOW_MAP_GEN].pl,
					1, 1, &r.tr_ds, 0, nullptr);
				VkDeviceSize offset = 0;
				vkCmdBindVertexBuffers(cb, 0, 1, &r.m.vb, &offset);
				vkCmdBindIndexBuffer(cb, r.m.ib, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cb, r.m.size, 1, 0, 0, 0);
			}

			if (vkEndCommandBuffer(cb) != VK_SUCCESS)
				throw std::runtime_error("failed to record command buffer!");
		}

		//gbuffer gen
		{
			auto cb = m_secondary_cbs[SECONDARY_CB_MAT_OPAQUE];

			VkCommandBufferInheritanceInfo inharitance = {};
			inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inharitance.framebuffer = m_fbs.frame_image_gen;
			inharitance.occlusionQueryEnable = VK_FALSE;
			inharitance.renderPass = m_passes[RENDER_PASS_FRAME_IMAGE_GEN];
			inharitance.subpass = render_pass_frame_image_gen::SUBPASS_GBUFFER_GEN;

			VkCommandBufferBeginInfo begin = {};
			begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin.pInheritanceInfo = &inharitance;

			if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS)
				throw std::runtime_error("failed to begin command buffer!");

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_GBUFFER_GEN].gp);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_GBUFFER_GEN].pl,
				0, 1, &m_gps[GP_GBUFFER_GEN].ds, 0, nullptr);

			for (auto& r : m_renderables[RENDERABLE_TYPE_MAT_OPAQUE])
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_GBUFFER_GEN].pl,
					1, 1, &r.tr_ds, 0, nullptr);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_GBUFFER_GEN].pl,
					2, 1, &r.mat_light_ds, 0, nullptr);
				std::array<VkBuffer, 2> vertex_buffers = { r.m.vb, r.m.veb };
				if (vertex_buffers[1] == VK_NULL_HANDLE)
					vertex_buffers[1] = r.m.vb;
				std::array<VkDeviceSize, 2> offsets = { 0,0 };
				vkCmdBindVertexBuffers(cb, 0, 2, vertex_buffers.data(), offsets.data());
				vkCmdBindIndexBuffer(cb, r.m.ib, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cb, r.m.size, 1, 0, 0, 0);
			}

			if (vkEndCommandBuffer(cb) != VK_SUCCESS)
				throw std::runtime_error("failed to record command buffer!");
		}
	}

	//record primary cb
	{
		auto cb = m_render_cb;

		VkCommandBufferBeginInfo cb_begin = {};
		cb_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cb_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(cb, &cb_begin) != VK_SUCCESS)
			throw std::runtime_error("failed to begin command buffer!");

		//copy data from staging buffer
		{
			VkBufferCopy region = {};
			region.dstOffset = 0;
			region.srcOffset = 0;
			region.size = m_res_data.size;

			vkCmdCopyBuffer(cb, m_res_data.staging_buffer, m_res_data.buffer, 1, &region);

			VkBufferMemoryBarrier barrier = {};
			barrier.buffer = m_res_data.buffer;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.offset = 0;
			barrier.size = m_res_data.size;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
			barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr,
				1, &barrier, 0, nullptr);
		}

		//environment map gen
		if(!m_renderables[RENDERABLE_TYPE_MAT_EM].empty() && !m_renderables[RENDERABLE_TYPE_SKYBOX].empty())
		{
			using namespace render_pass_environment_map_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.environment_map_gen_fb;
			begin.renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];

			std::array<VkClearValue, ATT_COUNT> clears = {};
			clears[ATT_DEPTH].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT_COUNT;
			begin.pClearValues = clears.data();
			begin.renderArea.extent.width = ENVIRONMENT_MAP_SIZE;
			begin.renderArea.extent.height = ENVIRONMENT_MAP_SIZE;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_MAT_EM]);
			vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_SKYBOX_EM]);
			vkCmdEndRenderPass(cb);
		}

		//dir shadow map gen
		if (!m_renderables[RENDERABLE_TYPE_MAT_OPAQUE].empty())
		{
			using namespace render_pass_dir_shadow_map_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.dir_shadow_map_gen_fb;
			begin.renderPass = m_passes[RENDER_PASS_DIR_SHADOW_MAP_GEN];

			std::array<VkClearValue, ATT_COUNT> clears = {};
			clears[ATT_DEPTH].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT_COUNT;
			begin.pClearValues = clears.data();
			begin.renderArea.extent.width = DIR_SHADOW_MAP_SIZE;
			begin.renderArea.extent.height = DIR_SHADOW_MAP_SIZE;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_DIR_SHADOW_GEN]);
			vkCmdEndRenderPass(cb);
		}

		//frame image gen
		{
			using namespace render_pass_frame_image_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.frame_image_gen;
			begin.renderPass = m_passes[RENDER_PASS_FRAME_IMAGE_GEN];

			std::array<VkClearValue, ATT_COUNT> clears = {};
			clears[ATT_DEPTHSTENCIL].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT_COUNT;
			begin.pClearValues = clears.data();
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			if(!m_renderables[RENDERABLE_TYPE_MAT_OPAQUE].empty())
				vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_MAT_OPAQUE]);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SS_DIR_SHADOW_MAP_GEN].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			//transition shadow map
			{
				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.image = m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.subresourceRange.levelCount = 1;

				vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &barrier);
			}

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SSAO_GEN].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			//transition ssao map
			{
				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.image = m_res_image[RES_IMAGE_SSAO_MAP].image;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.subresourceRange.levelCount = 1;

				vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &barrier);
			}

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SSAO_BLUR].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_IMAGE_ASSEMBLER].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdEndRenderPass(cb);
		}


		if (vkEndCommandBuffer(cb) != VK_SUCCESS)
			throw std::runtime_error("failed to record commnd buffer!");
	}

	//submit command buffers
	{
		std::array<VkSubmitInfo, 2> submit = {};
		submit[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit[0].commandBufferCount = 1;
		submit[0].pCommandBuffers = &m_render_cb;
		submit[0].pSignalSemaphores = &m_render_finished_s;
		submit[0].signalSemaphoreCount = 1;
		
		submit[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit[1].commandBufferCount = 1;
		submit[1].pCommandBuffers = &m_present_cbs[image_index];

		std::array<VkSemaphore, 2> wait_ss = { m_image_available_s, m_render_finished_s };
		submit[1].pWaitSemaphores = wait_ss.data();
		submit[1].waitSemaphoreCount = 2;
		std::array<VkPipelineStageFlags, 2> waits = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		submit[1].pWaitDstStageMask = waits.data();
		submit[1].pSignalSemaphores = &m_present_ready_ss[image_index];
		submit[1].signalSemaphoreCount = 1;
		
		if (vkQueueSubmit(m_base.graphics_queue, 2, submit.data(), m_render_finished_f) != VK_SUCCESS)
			throw std::runtime_error("failed to submit command buffers!");

	}

	//present swap chain image
	{
		VkPresentInfoKHR present = {};
		present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present.pImageIndices = &image_index;
		present.pSwapchains = &m_base.swap_chain;
		present.swapchainCount = 1;
		present.pWaitSemaphores = &m_present_ready_ss[image_index];
		present.waitSemaphoreCount = 1;
		
		if (vkQueuePresentKHR(m_base.present_queue, &present) != VK_SUCCESS)
			throw std::runtime_error("failed to present image!");
	}
}

void gta5_pass::process_settings(const render_settings & settings)
{
	auto projs = calc_projs(settings);

	//environment map gen mat
	{
		auto data = m_res_data.get<environment_map_gen_mat_data>();
		data->ambient_irradiance = settings.ambient_irradiance;
		data->dir = settings.light_dir;
		data->irradiance = settings.irradiance;
		data->view = settings.view;
	}

	//environment map gen skybox
	{
		auto data = m_res_data.get<environment_map_gen_skybox_data>();
		data->view = settings.view;
	}

	//dir shadow map gen
	{
		auto data = m_res_data.get<dir_shadow_map_gen_data>();
		auto projs = calc_projs(settings);
		std::array<glm::mat4, projs.size()> projs_from_world;
		for (uint32_t i = 0; i < projs.size(); ++i)
			projs_from_world[i] = projs[i] * settings.view;
		memcpy(data->projs, projs_from_world.data(), sizeof(glm::mat4)*projs.size());
	}

	//gbuffer gen
	{
		auto data = m_res_data.get<gbuffer_gen_data>();
		data->proj = settings.proj;
		data->view = settings.view;
	}

	//ss dir shadow map gen
	{
		auto data = m_res_data.get<ss_dir_shadow_map_gen_data>();
		data->far = settings.far;
		data->near = settings.near;
		memcpy(data->projs, projs.data(), sizeof(glm::mat4)*projs.size());
	}

	//image assembler data
	{
		auto data = m_res_data.get<image_assembler_data>();
		data->ambient_irradiance = settings.ambient_irradiance;
		data->dir = static_cast<glm::mat3>(settings.view)*settings.light_dir;
		data->irradiance = settings.irradiance;
	}

}

std::array<glm::mat4, FRUSTUM_SPLIT_COUNT> gta5_pass::calc_projs(const render_settings& settings)
{
	std::array<glm::mat4, FRUSTUM_SPLIT_COUNT> projs;

	glm::mat4 light = glm::lookAt(glm::vec3(0.f), settings.light_dir, glm::vec3(0.f, 1.f, 0.f));
	std::array<std::array<glm::vec3, 4>, FRUSTUM_SPLIT_COUNT + 1> frustum_points;
	
	glm::vec2 max[2];
	glm::vec2 min[2];

	for (uint32_t i = 0; i < FRUSTUM_SPLIT_COUNT + 1; ++i)
	{
		float z = settings.near + std::powf(static_cast<float>(i) / static_cast<float>(FRUSTUM_SPLIT_COUNT), 2.f)*(settings.far - settings.near);		
		
		std::array<float, 4> x = { -1.f, 1.f, -1.f, 1.f };
		std::array<float, 4> y = { -1.f, -1.f, 1.f, 1.f };

		max[i % 2] = glm::vec2(std::numeric_limits<float>::min());
		min[i % 2] = glm::vec2(std::numeric_limits<float>::max());

		for (uint32_t j=0; j<4; ++j)
		{
			glm::vec3 v = { x[j]*z / settings.view[0][0], y[j]*z / settings.view[1][1], z };
			v = static_cast<glm::mat3>(light)*v;
			frustum_points[i][j] = v;

			max[i % 2] = glm::max(max[i % 2], { v.x, v.y });
			min[i % 2] = glm::min(min[i % 2], { v.x, v.y });
		}

		if (i > 0)
		{
			glm::vec2 frustum_max = glm::max(max[0], max[1]);
			glm::vec2 frustum_min = glm::min(min[0], min[1]);

			glm::vec2 center = (frustum_max + frustum_min)*0.5f;
			glm::vec2 scale = glm::vec2(2.f)/(frustum_max - frustum_min);

			glm::mat4 proj(1.f);
			proj[0][0] = scale.x;
			proj[1][1] = scale.y;
			proj[2][2] = 0.5f / 10000.f;
			proj[3][0] = -scale.x*center.x;
			proj[3][1] = -scale.y*center.y;
			proj[3][2] = 0.5f;

			projs[i - 1] = proj;
		}
	}
	return projs; //from view space
}

void gta5_pass::wait_for_finish()
{
	vkWaitForFences(m_base.device, 1, &m_render_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
}