#include "gta5_pass.h"

#include "resource_manager.h"
#include "host_memory.h"
#include "device_memory.h"
#include "gta5_foundation.h"
#include "terrain_manager.h"

using namespace rcq;

gta5_pass* gta5_pass::m_instance = nullptr;

gta5_pass::gta5_pass(const base_info& info) : m_base(info)
{
	create_memory_resources_and_containers();
	create_resources();
	create_render_passes();
	create_dsls_and_allocate_dss();
	create_graphics_pipelines();
	create_compute_pipelines();
	create_command_pool();
	create_samplers();
	update_descriptor_sets();
	create_framebuffers();
	allocate_command_buffers();
	create_sync_objects();
	record_present_command_buffers();
}


gta5_pass::~gta5_pass()
{
	wait_for_finish();

	vkDestroyEvent(m_base.device, m_water_tex_ready_e, m_alloc);
	vkDestroyFence(m_base.device, m_render_finished_f, m_alloc);
	vkDestroyFence(m_base.device, m_compute_finished_f, m_alloc);
	vkDestroySemaphore(m_base.device, m_render_finished_s, m_alloc);
	vkDestroySemaphore(m_base.device, m_image_available_s, m_alloc);
	for (auto s : m_present_ready_ss)
		vkDestroySemaphore(m_base.device, s, m_alloc);

	vkDestroyFramebuffer(m_base.device, m_fbs.environment_map_gen, m_alloc);
	vkDestroyFramebuffer(m_base.device, m_fbs.dir_shadow_map_gen, m_alloc);
	vkDestroyFramebuffer(m_base.device, m_fbs.gbuffer_assembler, m_alloc);
	vkDestroyFramebuffer(m_base.device, m_fbs.ssao_map_gen, m_alloc);
	vkDestroyFramebuffer(m_base.device, m_fbs.preimage_assembler, m_alloc);
	for(auto fb : m_fbs.postprocessing)
		vkDestroyFramebuffer(m_base.device, fb, m_alloc);

	vkDestroyCommandPool(m_base.device, m_graphics_cp, m_alloc);
	vkDestroyCommandPool(m_base.device, m_compute_cp, m_alloc);
	vkDestroyCommandPool(m_base.device, m_present_cp, m_alloc);

	for (auto s : m_samplers)
		vkDestroySampler(m_base.device, s, m_alloc);

	for (auto& im : m_res_image)
	{
		vkDestroyImageView(m_base.device, im.view, m_alloc);
		vkDestroyImage(m_base.device, im.image, m_alloc);
	}

	vkUnmapMemory(m_base.device, m_res_data.staging_buffer_mem);
	vkDestroyBuffer(m_base.device, m_res_data.staging_buffer, m_alloc);
	vkDestroyBuffer(m_base.device, m_res_data.buffer, m_alloc);

	vkDestroyDescriptorPool(m_base.device, m_dp, m_alloc);

	for (auto& gp : m_gps)
	{
		vkDestroyPipeline(m_base.device, gp.gp, m_alloc);
		vkDestroyPipelineLayout(m_base.device, gp.pl, m_alloc);
		vkDestroyDescriptorSetLayout(m_base.device, gp.dsl, m_alloc);
	}

	for (auto& cp : m_cps)
	{
		vkDestroyPipeline(m_base.device, cp.gp, m_alloc);
		vkDestroyPipelineLayout(m_base.device, cp.pl, m_alloc);
		vkDestroyDescriptorSetLayout(m_base.device, cp.dsl, m_alloc);
	}
	for(auto pass : m_passes)
		vkDestroyRenderPass(m_base.device, pass, m_alloc);
}

void gta5_pass::create_memory_resources_and_containers()
{
	m_host_memory_resource.init(256 * 1024 * 1024, host_memory::instance()->resource()->max_alignment(),
		host_memory::instance()->resource());

	m_vk_alloc.init(&m_host_memory_resource);

	m_device_memory_resource.init(512 * 1024 * 1024, device_memory::instance()->resource()->max_alignment(),
		device_memory::instance()->resource(), &m_host_memory_resource);

	m_vk_mappable_memory_resource.init(m_base.device, device_memory::instance()->find_memory_type(~0,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), false, &m_vk_alloc);

	m_mappable_memory_resource.init(1024 * 1024, m_vk_mappable_memory_resource.max_alignment(), &m_vk_mappable_memory_resource,
		&m_host_memory_resource);

	m_opaque_objects.init(64, &m_host_memory_resource);
}

void gta5_pass::init(const base_info& info)
{
	assert(m_instance == nullptr);

	m_instance = new(reinterpret_cast<void*>(host_memory::instance()->resource()->allocate(sizeof(gta5_pass), alignof(gta5_pass))))
		gta5_pass(info);
}

void gta5_pass::destroy()
{
	assert(m_instance != nullptr);

	m_instance->~gta5_pass();
	host_memory::instance()->resource()->deallocate(reinterpret_cast<uint64_t>(m_instance));
}

void gta5_pass::create_render_passes()
{
	assert(vkCreateRenderPass(m_base.device, &render_pass_environment_map_gen::create_info,
		m_vk_alloc, &m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN]) == VK_SUCCESS);

	assert(vkCreateRenderPass(m_base.device, &render_pass_dir_shadow_map_gen::create_info,
		m_vk_alloc, &m_passes[RENDER_PASS_DIR_SHADOW_MAP_GEN]) == VK_SUCCESS);

	assert(vkCreateRenderPass(m_base.device, &render_pass_gbuffer_assembler::create_info,
		m_vk_alloc, &m_passes[RENDER_PASS_GBUFFER_ASSEMBLER]) == VK_SUCCESS);

	assert(vkCreateRenderPass(m_base.device, &render_pass_ssao_gen::create_info,
		m_vk_alloc, &m_passes[RENDER_PASS_SSAO_MAP_GEN]) == VK_SUCCESS);

	assert(vkCreateRenderPass(m_base.device, &render_pass_preimage_assembler::create_info,
		m_vk_alloc, &m_passes[RENDER_PASS_PREIMAGE_ASSEMBLER]) == VK_SUCCESS);

	assert(vkCreateRenderPass(m_base.device, &render_pass_postprocessing::create_info,
		m_vk_alloc, &m_passes[RENDER_PASS_POSTPROCESSING]) == VK_SUCCESS);

	assert(vkCreateRenderPass(m_base.device, &render_pass_refraction_image_gen::create_info,
		m_vk_alloc, &m_passes[RENDER_PASS_REFRACTION_IMAGE_GEN]) == VK_SUCCESS);

	assert(vkCreateRenderPass(m_base.device, &render_pass_water::create_info,
		m_vk_alloc, &m_passes[RENDER_PASS_WATER]) == VK_SUCCESS);
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

	render_pass_gbuffer_assembler::subpass_gbuffer_gen::pipeline_regular_mesh_drawer::runtime_info r3(m_base.device, m_base.swap_chain_image_extent);
	r3.fill_create_info(create_infos[GP_GBUFFER_GEN]);
	create_infos[GP_GBUFFER_GEN].renderPass = m_passes[RENDER_PASS_GBUFFER_ASSEMBLER];

	render_pass_gbuffer_assembler::subpass_ss_dir_shadow_map_gen::pipeline::runtime_info r4(m_base.device, m_base.swap_chain_image_extent);
	r4.fill_create_info(create_infos[GP_SS_DIR_SHADOW_MAP_GEN]);
	create_infos[GP_SS_DIR_SHADOW_MAP_GEN].renderPass = m_passes[RENDER_PASS_GBUFFER_ASSEMBLER];

	render_pass_preimage_assembler::subpass_ss_dir_shadow_map_blur::pipeline::runtime_info r5(m_base.device, m_base.swap_chain_image_extent);
	r5.fill_create_info(create_infos[GP_SS_DIR_SHADOW_MAP_BLUR]);
	create_infos[GP_SS_DIR_SHADOW_MAP_BLUR].renderPass = m_passes[RENDER_PASS_PREIMAGE_ASSEMBLER];

	render_pass_ssao_gen::subpass_unique::pipeline::runtime_info r6(m_base.device, m_base.swap_chain_image_extent);
	r6.fill_create_info(create_infos[GP_SSAO_GEN]);
	create_infos[GP_SSAO_GEN].renderPass = m_passes[RENDER_PASS_SSAO_MAP_GEN];

	render_pass_preimage_assembler::subpass_ssao_map_blur::pipeline::runtime_info r7(m_base.device, m_base.swap_chain_image_extent);
	r7.fill_create_info(create_infos[GP_SSAO_BLUR]);
	create_infos[GP_SSAO_BLUR].renderPass = m_passes[RENDER_PASS_PREIMAGE_ASSEMBLER];

	render_pass_preimage_assembler::subpass_image_assembler::pipeline::runtime_info r8(m_base.device, m_base.swap_chain_image_extent);
	r8.fill_create_info(create_infos[GP_IMAGE_ASSEMBLER]);
	create_infos[GP_IMAGE_ASSEMBLER].renderPass = m_passes[RENDER_PASS_PREIMAGE_ASSEMBLER];

	render_pass_preimage_assembler::subpass_sky_drawer::pipeline::runtime_info r9(m_base.device, m_base.swap_chain_image_extent);
	r9.fill_create_info(create_infos[GP_SKY_DRAWER]);
	create_infos[GP_SKY_DRAWER].renderPass = m_passes[RENDER_PASS_PREIMAGE_ASSEMBLER];

	render_pass_postprocessing::subpass_bypass::pipeline::runtime_info r10(m_base.device, m_base.swap_chain_image_extent);
	r10.fill_create_info(create_infos[GP_POSTPROCESSING]);
	create_infos[GP_POSTPROCESSING].renderPass = m_passes[RENDER_PASS_POSTPROCESSING];

	render_pass_preimage_assembler::subpass_sun_drawer::pipeline::runtime_info r11(m_base.device, m_base.swap_chain_image_extent);
	r11.fill_create_info(create_infos[GP_SUN_DRAWER]);
	create_infos[GP_SUN_DRAWER].renderPass = m_passes[RENDER_PASS_PREIMAGE_ASSEMBLER];
	
	render_pass_gbuffer_assembler::subpass_gbuffer_gen::pipeline_terrain_drawer::runtime_info r12(m_base.device, m_base.swap_chain_image_extent);
	r12.fill_create_info(create_infos[GP_TERRAIN_DRAWER]);
	create_infos[GP_TERRAIN_DRAWER].renderPass = m_passes[RENDER_PASS_GBUFFER_ASSEMBLER];

	render_pass_water::subpass_water_drawer::pipeline::runtime_info r13(m_base.device, m_base.swap_chain_image_extent);
	r13.fill_create_info(create_infos[GP_WATER_DRAWER]);
	create_infos[GP_WATER_DRAWER].renderPass = m_passes[RENDER_PASS_WATER];

	render_pass_refraction_image_gen::subpass_unique::pipeline::runtime_info r14(m_base.device, m_base.swap_chain_image_extent);
	r14.fill_create_info(create_infos[GP_REFRACTION_IMAGE_GEN]);
	create_infos[GP_REFRACTION_IMAGE_GEN].renderPass = m_passes[RENDER_PASS_REFRACTION_IMAGE_GEN];

	render_pass_preimage_assembler::subpass_ssr_ray_casting::pipeline::runtime_info r15(m_base.device, m_base.swap_chain_image_extent);
	r15.fill_create_info(create_infos[GP_SSR_RAY_CASTING]);
	create_infos[GP_SSR_RAY_CASTING].renderPass = m_passes[RENDER_PASS_PREIMAGE_ASSEMBLER];

	//create layouts
	m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].create_layout(m_base.device, 
	{
		resource_manager::instance()->get_dsl(DSL_TYPE_TR),
		resource_manager::instance()->get_dsl(DSL_TYPE_MAT_EM)
	}, m_vk_alloc);

	m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].create_layout(m_base.device, 
	{ 
		resource_manager::instance()->get_dsl(DSL_TYPE_SKYBOX) 
	}, m_vk_alloc);

	m_gps[GP_DIR_SHADOW_MAP_GEN].create_layout(m_base.device, 
	{
		resource_manager::instance()->get_dsl(DSL_TYPE_TR)
	}, m_vk_alloc);

	m_gps[GP_GBUFFER_GEN].create_layout(m_base.device, 
	{
		resource_manager::instance()->get_dsl(DSL_TYPE_TR),
		resource_manager::instance()->get_dsl(DSL_TYPE_MAT_OPAQUE)
	}, m_vk_alloc);

	m_gps[GP_SKY_DRAWER].create_layout(m_base.device,
	{
		resource_manager::instance()->get_dsl(DSL_TYPE_SKY)
	}, m_vk_alloc);

	m_gps[GP_SS_DIR_SHADOW_MAP_GEN].create_layout(m_base.device, {}, m_vk_alloc);
	m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].create_layout(m_base.device, {}, m_vk_alloc);
	m_gps[GP_SSAO_GEN].create_layout(m_base.device, {}, m_vk_alloc);
	m_gps[GP_SSAO_BLUR].create_layout(m_base.device, {}, m_vk_alloc);
	m_gps[GP_IMAGE_ASSEMBLER].create_layout(m_base.device, 
	{
		resource_manager::instance()->get_dsl(DSL_TYPE_SKY)
	}, m_vk_alloc);
	m_gps[GP_SUN_DRAWER].create_layout(m_base.device,
	{
		resource_manager::instance()->get_dsl(DSL_TYPE_SKY)
	}, m_vk_alloc);
	m_gps[GP_TERRAIN_DRAWER].create_layout(m_base.device,
	{
		resource_manager::instance()->get_dsl(DSL_TYPE_TERRAIN),
		resource_manager::instance()->get_dsl(DSL_TYPE_MAT_OPAQUE),
		resource_manager::instance()->get_dsl(DSL_TYPE_MAT_OPAQUE),
		resource_manager::instance()->get_dsl(DSL_TYPE_MAT_OPAQUE),
		resource_manager::instance()->get_dsl(DSL_TYPE_MAT_OPAQUE)
	}, m_vk_alloc);
	m_gps[GP_WATER_DRAWER].create_layout(m_base.device,
	{
		resource_manager::instance()->get_dsl(DSL_TYPE_WATER),
		resource_manager::instance()->get_dsl(DSL_TYPE_SKY)
	}, m_vk_alloc);
	m_gps[GP_REFRACTION_IMAGE_GEN].create_layout(m_base.device, {}, m_vk_alloc);
	m_gps[GP_SSR_RAY_CASTING].create_layout(m_base.device, {}, m_vk_alloc);
	m_gps[GP_POSTPROCESSING].create_layout(m_base.device, {}, m_vk_alloc);

	for (uint32_t i = 0; i < GP_COUNT; ++i)
		create_infos[i].layout = m_gps[i].pl;

	//create graphics pipelines 
	std::array<VkPipeline, GP_COUNT> gps;
	assert(vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, GP_COUNT, create_infos.data(), m_vk_alloc, gps.data())
		== VK_SUCCESS);

	/*for (uint32_t i = 0; i < GP_COUNT; ++i)
	{
		vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, 1, create_infos.data() + i, m_alloc, gps.data() + i);
		int k = 3;
	}*/

	for (uint32_t i = 0; i < GP_COUNT; ++i)
		m_gps[i].gp = gps[i];
}

void gta5_pass::create_compute_pipelines()
{
	std::array<VkComputePipelineCreateInfo, CP_COUNT> create_infos = {};

	compute_pipeline_terrain_tile_request::runtime_info r0(m_base.device);
	r0.fill_create_info(create_infos[CP_TERRAIN_TILE_REQUEST]);

	compute_pipeline_water_fft::runtime_info r1(m_base.device);
	r1.fill_create_info(create_infos[CP_WATER_FFT]);

	compute_pipeline_bloom_blur::runtime_info r2(m_base.device);
	r2.fill_create_info(create_infos[CP_BLOOM_BLUR]);

	//create_layouts
	m_cps[CP_TERRAIN_TILE_REQUEST].create_layout(m_base.device, 
	{
		resource_manager::instance()->get_dsl(DSL_TYPE_TERRAIN_COMPUTE)
	}, m_vk_alloc);
	m_cps[CP_WATER_FFT].create_layout(m_base.device,
	{
		resource_manager::instance()->get_dsl(DSL_TYPE_WATER_COMPUTE)
	}, m_vk_alloc, { {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t)} });

	m_cps[CP_BLOOM_BLUR].create_layout(m_base.device, {}, m_vk_alloc,
	{ {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t)} });
	for (uint32_t i = 0; i < CP_COUNT; ++i)
		create_infos[i].layout = m_cps[i].pl;

	//create compute pipelines
	std::array<VkPipeline, CP_COUNT> cps;
	assert(vkCreateComputePipelines(m_base.device, VK_NULL_HANDLE, CP_COUNT, create_infos.data(), m_vk_alloc, cps.data())
		== VK_SUCCESS);

	for (uint32_t i = 0; i < CP_COUNT; ++i)
		m_cps[i].gp = cps[i];
}


void gta5_pass::create_dsls_and_allocate_dss()
{
	//create dsls

	m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].create_dsl(m_base.device,
		render_pass_environment_map_gen::subpass_unique::pipelines::mat::dsl::create_info, m_vk_alloc);

	m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].create_dsl(m_base.device,
		render_pass_environment_map_gen::subpass_unique::pipelines::skybox::dsl::create_info, m_vk_alloc);

	m_gps[GP_DIR_SHADOW_MAP_GEN].create_dsl(m_base.device,
		render_pass_dir_shadow_map_gen::subpass_unique::pipeline::dsl::create_info, m_vk_alloc);


	m_gps[GP_GBUFFER_GEN].create_dsl(m_base.device,
		render_pass_gbuffer_assembler::subpass_gbuffer_gen::pipeline_regular_mesh_drawer::dsl::create_info, m_vk_alloc);

	m_gps[GP_SS_DIR_SHADOW_MAP_GEN].create_dsl(m_base.device,
		render_pass_gbuffer_assembler::subpass_ss_dir_shadow_map_gen::pipeline::dsl::create_info, m_vk_alloc);


	m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].create_dsl(m_base.device,
		render_pass_preimage_assembler::subpass_ss_dir_shadow_map_blur::pipeline::dsl::create_info, m_vk_alloc);

	m_gps[GP_SSAO_GEN].create_dsl(m_base.device,
		render_pass_ssao_gen::subpass_unique::pipeline::dsl::create_info, m_vk_alloc);

	m_gps[GP_SSAO_BLUR].create_dsl(m_base.device,
		render_pass_preimage_assembler::subpass_ssao_map_blur::pipeline::dsl::create_info, m_vk_alloc);

	m_gps[GP_IMAGE_ASSEMBLER].create_dsl(m_base.device,
		render_pass_preimage_assembler::subpass_image_assembler::pipeline::dsl::create_info, m_vk_alloc);

	m_gps[GP_POSTPROCESSING].create_dsl(m_base.device,
		render_pass_postprocessing::subpass_bypass::pipeline::dsl::create_info, m_vk_alloc);

	m_gps[GP_SKY_DRAWER].create_dsl(m_base.device,
		render_pass_preimage_assembler::subpass_sky_drawer::pipeline::dsl::create_info, m_vk_alloc);

	m_gps[GP_SUN_DRAWER].create_dsl(m_base.device,
		render_pass_preimage_assembler::subpass_sun_drawer::pipeline::dsl::create_info, m_vk_alloc);

	m_gps[GP_TERRAIN_DRAWER].create_dsl(m_base.device,
		render_pass_gbuffer_assembler::subpass_gbuffer_gen::pipeline_terrain_drawer::dsl::create_info, m_vk_alloc);

	m_gps[GP_WATER_DRAWER].create_dsl(m_base.device,
		render_pass_water::subpass_water_drawer::pipeline::dsl::create_info, m_vk_alloc);

	m_gps[GP_REFRACTION_IMAGE_GEN].create_dsl(m_base.device,
		render_pass_refraction_image_gen::subpass_unique::pipeline::dsl::create_info, m_vk_alloc);

	m_gps[GP_SSR_RAY_CASTING].create_dsl(m_base.device,
		render_pass_preimage_assembler::subpass_ssr_ray_casting::pipeline::dsl::create_info, m_vk_alloc);

	m_cps[CP_TERRAIN_TILE_REQUEST].create_dsl(m_base.device,
		compute_pipeline_terrain_tile_request::dsl::create_info, m_vk_alloc);

	m_cps[CP_WATER_FFT].create_dsl(m_base.device,
		compute_pipeline_water_fft::dsl::create_info, m_vk_alloc);

	m_cps[CP_BLOOM_BLUR].create_dsl(m_base.device,
		compute_pipeline_bloom_blur::dsl::create_info, m_vk_alloc);

	//create descriptor pool
	assert(vkCreateDescriptorPool(m_base.device, &dp::create_info,
		m_vk_alloc, &m_dp) == VK_SUCCESS);

	//allocate graphics dss
	{
		std::array<VkDescriptorSet, GP_COUNT> dss;
		std::array<VkDescriptorSetLayout, GP_COUNT> dsls;

		VkDescriptorSetAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = m_dp;
		alloc.descriptorSetCount = GP_COUNT;
		alloc.pSetLayouts = dsls.data();

		for (uint32_t i = 0; i < GP_COUNT; ++i)
			dsls[i] = m_gps[i].dsl;

		assert(vkAllocateDescriptorSets(m_base.device, &alloc, dss.data()) == VK_SUCCESS);

		for (uint32_t i = 0; i < GP_COUNT; ++i)
			m_gps[i].ds = dss[i];
	}
	//allocate compute dss
	{
		std::array<VkDescriptorSet, CP_COUNT> dss;
		std::array<VkDescriptorSetLayout, CP_COUNT> dsls;

		VkDescriptorSetAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = m_dp;
		alloc.descriptorSetCount = CP_COUNT;
		alloc.pSetLayouts = dsls.data();

		for (uint32_t i = 0; i < CP_COUNT; ++i)
			dsls[i] = m_cps[i].dsl;

		assert(vkAllocateDescriptorSets(m_base.device, &alloc, dss.data()) == VK_SUCCESS);

		for (uint32_t i = 0; i < CP_COUNT; ++i)
			m_cps[i].ds = dss[i];
	}
}

void gta5_pass::create_resources()
{
	/*std::vector<VkMemoryAllocateInfo> alloc_infos(MEMORY_COUNT);
	for (auto& alloc : alloc_infos)
		alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;*/


	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(m_base.physical_device, &props);
	uint64_t ub_alignment = props.limits.minUniformBufferOffsetAlignment;
	m_res_data.calc_offset_and_size(ub_alignment);

	size_t size = m_res_data.size;

	//staging buffer
	{
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer.size = size;
		buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		assert(vkCreateBuffer(m_base.device, &buffer, m_vk_alloc, &m_res_data.staging_buffer) == VK_SUCCESS);
		
		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, m_res_data.staging_buffer, &mr);
		m_res_data.staging_buffer_offset = m_mappable_memory_resource.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, m_res_data.staging_buffer, m_mappable_memory_resource.handle(), 
			m_res_data.staging_buffer_offset);
		m_res_data.set_pointers(m_mappable_memory_resource.map(m_res_data.staging_buffer_offset, size));
	}

	//buffer
	{
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer.size = size;
		buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		assert(vkCreateBuffer(m_base.device, &buffer, m_vk_alloc, &m_res_data.buffer) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetBufferMemoryRequirements(m_base.device, m_res_data.buffer, &mr);
		m_res_data.buffer_offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindBufferMemory(m_base.device, m_res_data.buffer, m_device_memory_resource.handle(), m_res_data.buffer_offset);
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

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image)
			== VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image, m_device_memory_resource.handle(),
			offset);

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

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].view)
			== VK_SUCCESS);
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

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image)
			== VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image, m_device_memory_resource.handle(),
			offset);

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

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_ENVIRONMENT_MAP].view)
			== VK_SUCCESS);
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

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image)
			== VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image, m_device_memory_resource.handle(),
			offset);

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

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view)
			== VK_SUCCESS);
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

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image)
			== VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image, m_device_memory_resource.handle(),
			offset);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].view)
			== VK_SUCCESS);
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

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image)
			== VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image, m_device_memory_resource.handle(),
			offset);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_GB_METALNESS_SSDS].view)
			== VK_SUCCESS);
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
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_GB_NORMAL_AO].image)
			== VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_NORMAL_AO].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_NORMAL_AO].image, m_device_memory_resource.handle(),
			offset);

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

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_GB_NORMAL_AO].view)
			== VK_SUCCESS);
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
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT 
			| VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_PREIMAGE].image)
			== VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_PREIMAGE].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_PREIMAGE].image, m_device_memory_resource.handle(),
			offset);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_PREV_IMAGE].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_PREV_IMAGE].view) == VK_SUCCESS);
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

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_GB_DEPTH].image)
			== VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_GB_DEPTH].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_DEPTH].image, m_device_memory_resource.handle(),
			offset);

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

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_GB_DEPTH].view)
			== VK_SUCCESS);
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

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image)
			== VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image, m_device_memory_resource.handle(),
			offset);

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

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_DIR_SHADOW_MAP].view)
			== VK_SUCCESS);
	}

	//prev_image
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.depth = 1;
		image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_PREV_IMAGE].image) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_PREV_IMAGE].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_PREV_IMAGE].image, m_device_memory_resource.handle(),
			offset);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_PREV_IMAGE].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_PREV_IMAGE].view) == VK_SUCCESS);
	}

	//refraction image
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.depth = 1;
		image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_REFRACTION_IMAGE].image) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_REFRACTION_IMAGE].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_REFRACTION_IMAGE].image, m_device_memory_resource.handle(),
			offset);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_REFRACTION_IMAGE].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_REFRACTION_IMAGE].view) == VK_SUCCESS);
	}

	//ssr ray casting coords
	{
		VkImageCreateInfo image = {};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.arrayLayers = 1;
		image.arrayLayers = 1;
		image.extent.width = m_base.swap_chain_image_extent.width;
		image.extent.height = m_base.swap_chain_image_extent.height;
		image.extent.depth = 1;
		image.format = VK_FORMAT_R32_SINT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].image) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].image, m_device_memory_resource.handle(),
			offset);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32_SINT;
		view.image = m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.levelCount = 1;

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].view) == VK_SUCCESS);
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

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image)
			== VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image, m_device_memory_resource.handle(),
			offset);

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

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].view)
			== VK_SUCCESS);
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

		assert(vkCreateImage(m_base.device, &image, m_vk_alloc, &m_res_image[RES_IMAGE_SSAO_MAP].image)
			!= VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_SSAO_MAP].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_SSAO_MAP].image, m_device_memory_resource.handle(),
			offset);

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

		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_SSAO_MAP].view)
			== VK_SUCCESS);
	}

	//bloom blur
	{
		VkImageCreateInfo im = {};
		im.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		im.arrayLayers = 2;
		im.extent.width = m_base.swap_chain_image_extent.width / BLOOM_IMAGE_SIZE_FACTOR;
		im.extent.height = m_base.swap_chain_image_extent.height / BLOOM_IMAGE_SIZE_FACTOR;
		im.extent.depth = 1;
		im.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		im.imageType = VK_IMAGE_TYPE_2D;
		im.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		im.mipLevels = 1;
		im.samples = VK_SAMPLE_COUNT_1_BIT;
		im.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		im.tiling = VK_IMAGE_TILING_OPTIMAL;
		im.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		assert(vkCreateImage(m_base.device, &im, m_vk_alloc, &m_res_image[RES_IMAGE_BLOOM_BLUR].image) == VK_SUCCESS);

		VkMemoryRequirements mr;
		vkGetImageMemoryRequirements(m_base.device, m_res_image[RES_IMAGE_BLOOM_BLUR].image, &mr);
		uint64_t offset = m_device_memory_resource.allocate(mr.size, mr.alignment);
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_BLOOM_BLUR].image, m_device_memory_resource.handle(),
			offset);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.layerCount = 2;
		view.subresourceRange.levelCount = 1;

		view.image = m_res_image[RES_IMAGE_BLOOM_BLUR].image;
		assert(vkCreateImageView(m_base.device, &view, m_vk_alloc, &m_res_image[RES_IMAGE_BLOOM_BLUR].view) == VK_SUCCESS);
	}
}

void gta5_pass::create_command_pool()
{
	VkCommandPoolCreateInfo pool = {};
	pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool.queueFamilyIndex = m_base.queue_families.graphics_family;
	assert(vkCreateCommandPool(m_base.device, &pool, m_vk_alloc, &m_graphics_cp) == VK_SUCCESS);

	pool.queueFamilyIndex = m_base.queue_families.present_family;
	assert(vkCreateCommandPool(m_base.device, &pool, m_vk_alloc, &m_present_cp) != VK_SUCCESS);

	pool.queueFamilyIndex = m_base.queue_families.compute_family;
	assert(vkCreateCommandPool(m_base.device, &pool, m_vk_alloc, &m_compute_cp) != VK_SUCCESS);
}

void gta5_pass::update_descriptor_sets()
{
	std::vector<VkWriteDescriptorSet> w;
	
	//environment map gen mat
	{
		VkDescriptorBufferInfo ub = {};
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_ENVIRONMENT_MAP_GEN_MAT];
		ub.range = sizeof(resource_data<RES_DATA_ENVIRONMENT_MAP_GEN_MAT>);

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
		ub.offset = m_res_data.offsets[RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX];
		ub.range = sizeof(resource_data<RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX>);

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
		ub.offset = m_res_data.offsets[RES_DATA_GBUFFER_GEN];
		ub.range = sizeof(resource_data<RES_DATA_GBUFFER_GEN>);

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
		ub.offset = m_res_data.offsets[RES_DATA_DIR_SHADOW_MAP_GEN];
		ub.range = sizeof(resource_data<RES_DATA_DIR_SHADOW_MAP_GEN>);

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
		ub.offset = m_res_data.offsets[RES_DATA_SS_DIR_SHADOW_MAP_GEN];
		ub.range = sizeof(resource_data<RES_DATA_SS_DIR_SHADOW_MAP_GEN>);

		VkDescriptorImageInfo shadow_tex = {};
		shadow_tex.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadow_tex.imageView = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].view;
		shadow_tex.sampler = m_samplers[SAMPLER_GENERAL];
		
		VkDescriptorImageInfo pos = {};
		pos.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;

		VkDescriptorImageInfo normal = {};
		normal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normal.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;

		w.resize(4);
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

		w[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[3].descriptorCount = 1;
		w[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[3].dstArrayElement = 0;
		w[3].dstBinding = 3;
		w[3].dstSet = m_gps[GP_SS_DIR_SHADOW_MAP_GEN].ds;
		w[3].pImageInfo = &normal;

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
		normal.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

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
		w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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
		normal.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

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
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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

	//ssr ray casting
	{
		VkDescriptorBufferInfo data;
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_SSR_RAY_CASTING];
		data.range = sizeof(resource_data<RES_DATA_SSR_RAY_CASTING>);

		VkDescriptorImageInfo normal_tex;
		normal_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normal_tex.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		normal_tex.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo pos_tex;
		pos_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos_tex.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		pos_tex.sampler= m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo ray_end_fragment_ids;
		ray_end_fragment_ids.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		ray_end_fragment_ids.imageView = m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].view;

		w.resize(4);

		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_SSR_RAY_CASTING].ds;
		w[0].pBufferInfo = &data;

		w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[1].descriptorCount = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstArrayElement = 0;
		w[1].dstBinding = 1;
		w[1].dstSet = m_gps[GP_SSR_RAY_CASTING].ds;
		w[1].pImageInfo = &normal_tex;

		w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[2].descriptorCount = 1;
		w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[2].dstArrayElement = 0;
		w[2].dstBinding = 2;
		w[2].dstSet = m_gps[GP_SSR_RAY_CASTING].ds;
		w[2].pImageInfo = &pos_tex;

		w[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[3].descriptorCount = 1;
		w[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		w[3].dstArrayElement = 0;
		w[3].dstBinding = 3;
		w[3].dstSet = m_gps[GP_SSR_RAY_CASTING].ds;
		w[3].pImageInfo = &ray_end_fragment_ids;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//refraction map gen
	{
		VkDescriptorBufferInfo data;
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_REFRACTION_MAP_GEN];
		data.range = sizeof(resource_data<RES_DATA_REFRACTION_MAP_GEN>);

		VkDescriptorImageInfo preimage;
		preimage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		preimage.imageView = m_res_image[RES_IMAGE_PREIMAGE].view;
		preimage.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		w.resize(2);

		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_REFRACTION_IMAGE_GEN].ds;
		w[0].pBufferInfo = &data;

		w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[1].descriptorCount = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstArrayElement = 0;
		w[1].dstBinding = 1;
		w[1].dstSet = m_gps[GP_REFRACTION_IMAGE_GEN].ds;
		w[1].pImageInfo = &preimage;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}
	
	//image assembler
	{
		VkDescriptorImageInfo gb_pos_roughness;
		gb_pos_roughness.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_pos_roughness.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		gb_pos_roughness.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo gb_BASECOLOR_SSAO;
		gb_BASECOLOR_SSAO.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_BASECOLOR_SSAO.imageView = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].view;

		VkDescriptorImageInfo gb_METALNESS_SSDS;
		gb_METALNESS_SSDS.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_METALNESS_SSDS.imageView = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].view;
		
		VkDescriptorImageInfo gb_normal_ao;
		gb_normal_ao.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gb_normal_ao.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		gb_normal_ao.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo em;
		em.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		em.imageView = m_res_image[RES_IMAGE_ENVIRONMENT_MAP].view;
		//em.imageView = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].view;
		em.sampler = m_samplers[SAMPLER_GENERAL];

		VkDescriptorBufferInfo ub;
		ub.buffer = m_res_data.buffer;
		ub.offset = m_res_data.offsets[RES_DATA_IMAGE_ASSEMBLER];
		ub.range = sizeof(resource_data<RES_DATA_IMAGE_ASSEMBLER>);

		VkDescriptorImageInfo prev_image;
		prev_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		prev_image.imageView = m_res_image[RES_IMAGE_PREV_IMAGE].view;
		prev_image.sampler = m_samplers[SAMPLER_GENERAL];

		VkDescriptorImageInfo ssr_ray_casting_coords;
		ssr_ray_casting_coords.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		ssr_ray_casting_coords.imageView = m_res_image[RES_IMAGE_SSR_RAY_CASTING_COORDS].view;

		w.resize(8);

		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[0].pImageInfo = &gb_BASECOLOR_SSAO;

		w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[1].descriptorCount = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		w[1].dstArrayElement = 0;
		w[1].dstBinding = 1;
		w[1].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[1].pImageInfo = &gb_METALNESS_SSDS;

		w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[2].descriptorCount = 1;
		w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[2].dstArrayElement = 0;
		w[2].dstBinding = 2;
		w[2].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[2].pImageInfo = &gb_pos_roughness;

		w[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[3].descriptorCount = 1;
		w[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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

		w[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[6].descriptorCount = 1;
		w[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[6].dstArrayElement = 0;
		w[6].dstBinding = 6;
		w[6].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[6].pImageInfo = &prev_image;

		w[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[7].descriptorCount = 1;
		w[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		w[7].dstArrayElement = 0;
		w[7].dstBinding = 7;
		w[7].dstSet = m_gps[GP_IMAGE_ASSEMBLER].ds;
		w[7].pImageInfo = &ssr_ray_casting_coords;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//sky drawer
	{
		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_SKY_DRAWER];
		data.range = sizeof(resource_data<RES_DATA_SKY_DRAWER>);

		w.resize(1);
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_SKY_DRAWER].ds;
		w[0].pBufferInfo = &data;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//sun drawer
	{
		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_SUN_DRAWER];
		data.range = sizeof(resource_data<RES_DATA_SUN_DRAWER>);

		w.resize(1);
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_SUN_DRAWER].ds;
		w[0].pBufferInfo = &data;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//terrain drawer
	{
		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_TERRAIN_DRAWER];
		data.range = sizeof(resource_data<RES_DATA_TERRAIN_DRAWER>);

		w.resize(1);
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_TERRAIN_DRAWER].ds;
		w[0].pBufferInfo = &data;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//terrain tile request
	{
		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_TERRAIN_TILE_REQUEST];
		data.range = sizeof(resource_data<RES_DATA_TERRAIN_TILE_REQUEST>);

		w.resize(1);
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_cps[CP_TERRAIN_TILE_REQUEST].ds;
		w[0].pBufferInfo = &data;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//water drawer
	{

		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_WATER_DRAWER];
		data.range = sizeof(resource_data<RES_DATA_WATER_DRAWER>);

		VkDescriptorImageInfo refr_tex;
		refr_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		refr_tex.imageView = m_res_image[RES_IMAGE_REFRACTION_IMAGE].view;
		refr_tex.sampler = m_samplers[SAMPLER_GENERAL];

		VkDescriptorImageInfo pos_tex;
		pos_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pos_tex.imageView = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		pos_tex.sampler = m_samplers[SAMPLER_GENERAL];

		VkDescriptorImageInfo normal_tex;
		normal_tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normal_tex.imageView = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		normal_tex.sampler = m_samplers[SAMPLER_GENERAL];

		w.resize(4);
		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_WATER_DRAWER].ds;
		w[0].pBufferInfo = &data;

		w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[1].descriptorCount = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstArrayElement = 0;
		w[1].dstBinding = 1;
		w[1].dstSet = m_gps[GP_WATER_DRAWER].ds;
		w[1].pImageInfo = &refr_tex;

		w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[2].descriptorCount = 1;
		w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[2].dstArrayElement = 0;
		w[2].dstBinding = 2;
		w[2].dstSet = m_gps[GP_WATER_DRAWER].ds;
		w[2].pImageInfo = &pos_tex;

		w[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[3].descriptorCount = 1;
		w[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[3].dstArrayElement = 0;
		w[3].dstBinding = 3;
		w[3].dstSet = m_gps[GP_WATER_DRAWER].ds;
		w[3].pImageInfo = &normal_tex;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//water compute
	{

		VkDescriptorBufferInfo data = {};
		data.buffer = m_res_data.buffer;
		data.offset = m_res_data.offsets[RES_DATA_WATER_COMPUTE];
		data.range = sizeof(resource_data<RES_DATA_WATER_COMPUTE>);

		w.resize(1);

		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_cps[CP_WATER_FFT].ds;
		w[0].pBufferInfo = &data;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}
	//bloom blur
	{
		VkDescriptorImageInfo blur_im;
		blur_im.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		blur_im.imageView = m_res_image[RES_IMAGE_BLOOM_BLUR].view;

		w.resize(1);

		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_cps[CP_BLOOM_BLUR].ds;
		w[0].pImageInfo = &blur_im;

		vkUpdateDescriptorSets(m_base.device, w.size(), w.data(), 0, nullptr);
	}

	//postprocessing
	{
		VkDescriptorImageInfo preimage = {};
		preimage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		preimage.imageView = m_res_image[RES_IMAGE_PREIMAGE].view;
		preimage.sampler = m_samplers[SAMPLER_UNNORMALIZED_COORD];

		VkDescriptorImageInfo bloom_blur;
		bloom_blur.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		bloom_blur.imageView = m_res_image[RES_IMAGE_BLOOM_BLUR].view;
		bloom_blur.sampler = m_samplers[SAMPLER_GENERAL];

		w.resize(2);

		w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[0].descriptorCount = 1;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[0].dstArrayElement = 0;
		w[0].dstBinding = 0;
		w[0].dstSet = m_gps[GP_POSTPROCESSING].ds;
		w[0].pImageInfo = &preimage;

		w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[1].descriptorCount = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].dstArrayElement = 0;
		w[1].dstBinding = 1;
		w[1].dstSet = m_gps[GP_POSTPROCESSING].ds;
		w[1].pImageInfo = &bloom_blur;

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

		assert(vkCreateSampler(m_base.device, &sampler, m_vk_alloc, &m_samplers[SAMPLER_GENERAL]) == VK_SUCCESS);
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

		assert(vkCreateSampler(m_base.device, &sampler, m_vk_alloc, &m_samplers[SAMPLER_UNNORMALIZED_COORD]) == VK_SUCCESS);
	}
}

void gta5_pass::create_sync_objects()
{
	VkSemaphoreCreateInfo s = {};
	s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	assert(vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &m_image_available_s) == VK_SUCCESS &&
		vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &m_render_finished_s) == VK_SUCCESS &&
		vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &m_bloom_blur_ready_s) == VK_SUCCESS &&
		vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &m_preimage_ready_s) == VK_SUCCESS);

	for (auto& sem : m_present_ready_ss)
		assert(vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &sem) == VK_SUCCESS);

	VkFenceCreateInfo f = {};
	f.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	f.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	assert(vkCreateFence(m_base.device, &f, m_vk_alloc, &m_render_finished_f) == VK_SUCCESS &&
		vkCreateFence(m_base.device, &f, m_vk_alloc, &m_compute_finished_f) == VK_SUCCESS);

	VkEventCreateInfo e = {};
	e.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	assert(vkCreateEvent(m_base.device, &e, m_vk_alloc, &m_water_tex_ready_e) == VK_SUCCESS);

}

void gta5_pass::create_framebuffers()
{
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

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs.environment_map_gen) == VK_SUCCESS);
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

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs.dir_shadow_map_gen) == VK_SUCCESS);
	}

	//gbuffer assembler
	{
		using namespace render_pass_gbuffer_assembler;

		std::array<VkImageView, ATT_COUNT> atts;
		atts[ATT_POS_ROUGHNESS] = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].view;
		atts[ATT_METALNESS_SSDS] = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].view;
		atts[ATT_BASECOLOR_SSAO] = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].view;
		atts[ATT_NORMAL_AO] = m_res_image[RES_IMAGE_GB_NORMAL_AO].view;
		atts[ATT_DEPTHSTENCIL] = m_res_image[RES_IMAGE_GB_DEPTH].view;
		atts[ATT_SS_DIR_SHADOW_MAP] = m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT_COUNT;
		fb.pAttachments = atts.data();
		fb.height = m_base.swap_chain_image_extent.height;
		fb.width = m_base.swap_chain_image_extent.width;
		fb.layers = 1;
		fb.renderPass = m_passes[RENDER_PASS_GBUFFER_ASSEMBLER];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs.gbuffer_assembler) == VK_SUCCESS);
	}

	//ssao map gen
	{
		using namespace render_pass_ssao_gen;

		std::array<VkImageView, ATT_COUNT> atts;
		atts[ATT_SSAO_MAP] = m_res_image[RES_IMAGE_SSAO_MAP].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT_COUNT;
		fb.pAttachments = atts.data();
		fb.height = m_base.swap_chain_image_extent.height;
		fb.width = m_base.swap_chain_image_extent.width;
		fb.layers = 1;
		fb.renderPass = m_passes[RENDER_PASS_SSAO_MAP_GEN];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs.ssao_map_gen) == VK_SUCCESS);
	}

	//preimage assembler
	{
		using namespace render_pass_preimage_assembler;

		std::array<VkImageView, ATT_COUNT> atts;
		atts[ATT_METALNESS_SSDS] = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].view;
		atts[ATT_BASECOLOR_SSAO] = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].view;
		atts[ATT_PREIMAGE] = m_res_image[RES_IMAGE_PREIMAGE].view;
		atts[ATT_DEPTHSTENCIL] = m_res_image[RES_IMAGE_GB_DEPTH].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT_COUNT;
		fb.pAttachments = atts.data();
		fb.height = m_base.swap_chain_image_extent.height;
		fb.width = m_base.swap_chain_image_extent.width;
		fb.layers = 1;
		fb.renderPass = m_passes[RENDER_PASS_PREIMAGE_ASSEMBLER];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs.preimage_assembler) == VK_SUCCESS);
	}

	//refraction map gen
	{
		using namespace render_pass_refraction_image_gen;

		std::array<VkImageView, ATT_COUNT> atts;
		atts[ATT_DEPTHSTENCIL] = m_res_image[RES_IMAGE_GB_DEPTH].view;
		atts[ATT_REFRACTION_IMAGE] = m_res_image[RES_IMAGE_REFRACTION_IMAGE].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT_COUNT;
		fb.pAttachments = atts.data();
		fb.height = m_base.swap_chain_image_extent.height;
		fb.width = m_base.swap_chain_image_extent.width;
		fb.layers = 1;
		fb.renderPass = m_passes[RENDER_PASS_REFRACTION_IMAGE_GEN];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs.refraction_image_gen) == VK_SUCCESS);
	}

	//water
	{
		using namespace render_pass_water;

		std::array<VkImageView, ATT_COUNT> atts;
		atts[ATT_FRAME_IMAGE] = m_res_image[RES_IMAGE_PREIMAGE].view;
		atts[ATT_DEPTHSTENCIL] = m_res_image[RES_IMAGE_GB_DEPTH].view;

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = ATT_COUNT;
		fb.pAttachments = atts.data();
		fb.height = m_base.swap_chain_image_extent.height;
		fb.width = m_base.swap_chain_image_extent.width;
		fb.layers = 1;
		fb.renderPass = m_passes[RENDER_PASS_WATER];

		assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs.water) == VK_SUCCESS);
	}

	//postprocessing
	{

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

		for (uint32_t i=0; i<SWAP_CHAIN_SIZE; ++i)
		{
			atts[ATT_SWAP_CHAIN_IMAGE] = m_base.swap_chain_image_views[i];
			atts[ATT_PREV_IMAGE] = m_res_image[RES_IMAGE_PREV_IMAGE].view;

			assert(vkCreateFramebuffer(m_base.device, &fb, m_vk_alloc, &m_fbs.postprocessing[i]) == VK_SUCCESS);
		}
	}
}

void gta5_pass::allocate_command_buffers()
{
	//primary cb
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = 1;
		alloc.commandPool = m_graphics_cp;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_render_cb) == VK_SUCCESS);

	}

	//present cbs
	{

		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = SWAP_CHAIN_SIZE;
		alloc.commandPool = m_graphics_cp;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, m_present_cbs) == VK_SUCCESS);
	}
		
	//compute cbs
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = 1;
		alloc.commandPool = m_compute_cp;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_terrain_request_cb) == VK_SUCCESS);
		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_water_fft_cb) == VK_SUCCESS);
		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_bloom_blur_cb) == VK_SUCCESS);
	}

	//secondary cbs
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = SECONDARY_CB_COUNT;
		alloc.commandPool = m_graphics_cp;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, m_secondary_cbs) == VK_SUCCESS);
	}
}

void gta5_pass::record_present_command_buffers()
{
	for (uint32_t i = 0; i < SWAP_CHAIN_SIZE; ++i)
	{
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		assert(vkBeginCommandBuffer(m_present_cbs[i], &begin) == VK_SUCCESS);

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

		assert(vkEndCommandBuffer(m_present_cbs[i]) == VK_SUCCESS);
	}

	//record bloom blur cb
	{
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		assert(vkBeginCommandBuffer(m_bloom_blur_cb, &begin) == VK_SUCCESS);

		glm::ivec2 size = { m_base.swap_chain_image_extent.width / BLOOM_IMAGE_SIZE_FACTOR, 
			m_base.swap_chain_image_extent.height / BLOOM_IMAGE_SIZE_FACTOR };

		m_cps[CP_BLOOM_BLUR].bind(m_bloom_blur_cb, VK_PIPELINE_BIND_POINT_COMPUTE);
		uint32_t step = 0;
		vkCmdPushConstants(m_bloom_blur_cb, m_cps[CP_BLOOM_BLUR].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &step);
		vkCmdDispatch(m_bloom_blur_cb, size.x, size.y, 1);

		for (uint32_t i = 0; i < 1; ++i)
		{
			VkMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(m_bloom_blur_cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 1, &b, 0, nullptr, 0, nullptr);

			step = 1;
			vkCmdPushConstants(m_bloom_blur_cb, m_cps[CP_BLOOM_BLUR].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &step);
			vkCmdDispatch(m_bloom_blur_cb, size.x, size.y, 1);

			vkCmdPipelineBarrier(m_bloom_blur_cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 1, &b, 0, nullptr, 0, nullptr);

			step = 2;
			vkCmdPushConstants(m_bloom_blur_cb, m_cps[CP_BLOOM_BLUR].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &step);
			vkCmdDispatch(m_bloom_blur_cb, size.x, size.y, 1);
		}

		//barrier for bloom blur image
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.image = m_res_image[RES_IMAGE_BLOOM_BLUR].image;
			b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 2;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(m_bloom_blur_cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}
		assert(vkEndCommandBuffer(m_bloom_blur_cb) == VK_SUCCESS);
	}
}


void gta5_pass::render(/*const render_settings & settings, std::bitset<RENDERABLE_TYPE_COUNT> record_mask*/)
{
	timer t;
	t.start();

	process_settings();

	if (m_opaque_objects.size()!=0)
	{
		auto cb = m_secondary_cbs[SECONDARY_CB_MAT_EM];

		VkCommandBufferInheritanceInfo inharitance = {};
		inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inharitance.framebuffer = m_fbs.environment_map_gen;
		inharitance.occlusionQueryEnable = VK_FALSE;
		inharitance.renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];
		inharitance.subpass = 0;

		VkCommandBufferBeginInfo begin = {};
		begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT|VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.pInheritanceInfo = &inharitance;

		assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].gp);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].pl,
			0, 1, &m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].ds, 0, nullptr);

		/*m_opaque_objects.for_each([cb, pl=m_gps[GP_ENVIRONMENT_MAP_GEN_MAT]])


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
		}*/

		assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
	}
	/*if (record_mask[RENDERABLE_TYPE_SKYBOX] && !m_renderables[RENDERABLE_TYPE_SKYBOX].empty())
	{
		auto cb = m_secondary_cbs[SECONDARY_CB_SKYBOX_EM];

		VkCommandBufferInheritanceInfo inharitance = {};
		inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inharitance.framebuffer = m_fbs.environment_map_gen;
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
	}*/

	if (m_opaque_objects.size()!=0)
	{
		//shadow map gen
		{
			auto cb = m_secondary_cbs[SECONDARY_CB_DIR_SHADOW_GEN];

			VkCommandBufferInheritanceInfo inharitance = {};
			inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inharitance.framebuffer = m_fbs.dir_shadow_map_gen;
			inharitance.occlusionQueryEnable = VK_FALSE;
			inharitance.renderPass = m_passes[RENDER_PASS_DIR_SHADOW_MAP_GEN];
			inharitance.subpass = 0;

			VkCommandBufferBeginInfo begin = {};
			begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin.pInheritanceInfo = &inharitance;

			assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_DIR_SHADOW_MAP_GEN].gp);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_DIR_SHADOW_MAP_GEN].pl,
				0, 1, &m_gps[GP_DIR_SHADOW_MAP_GEN].ds, 0, nullptr);

			m_opaque_objects.for_each([cb, pl = m_gps[GP_DIR_SHADOW_MAP_GEN].pl](auto&& obj)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pl,
					1, 1, &obj.tr_ds, 0, nullptr);
				VkDeviceSize offset = 0;
				vkCmdBindVertexBuffers(cb, 0, 1, &obj.mesh_vb, &offset);
				vkCmdBindIndexBuffer(cb, obj.mesh_ib, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cb, obj.mesh_index_size, 1, 0, 0, 0);
			});

			assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
		}

		//gbuffer gen
		{
			auto cb = m_secondary_cbs[SECONDARY_CB_MAT_OPAQUE];

			VkCommandBufferInheritanceInfo inharitance = {};
			inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inharitance.framebuffer = m_fbs.gbuffer_assembler;
			inharitance.occlusionQueryEnable = VK_FALSE;
			inharitance.renderPass = m_passes[RENDER_PASS_GBUFFER_ASSEMBLER];
			inharitance.subpass = render_pass_gbuffer_assembler::SUBPASS_GBUFFER_GEN;

			VkCommandBufferBeginInfo begin = {};
			begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin.pInheritanceInfo = &inharitance;

			assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_GBUFFER_GEN].gp);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_GBUFFER_GEN].pl,
				0, 1, &m_gps[GP_GBUFFER_GEN].ds, 0, nullptr);

			m_opaque_objects.for_each([cb, pl = m_gps[GP_GBUFFER_GEN].pl](auto&& obj)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,pl,
					1, 1, &obj.tr_ds, 0, nullptr);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pl,
					2, 1, &obj.opaque_material_ds, 0, nullptr);
				std::array<VkBuffer, 2> vertex_buffers = { obj.mesh_vb, obj.mesh_veb };
				if (vertex_buffers[1] == VK_NULL_HANDLE)
					vertex_buffers[1] = obj.mesh_vb;
				std::array<VkDeviceSize, 2> offsets = { 0,0 };
				vkCmdBindVertexBuffers(cb, 0, 2, vertex_buffers.data(), offsets.data());
				vkCmdBindIndexBuffer(cb, obj.mesh_ib, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cb, obj.mesh_index_size, 1, 0, 0, 0);
			});

			assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
		}
	}

	vkWaitForFences(m_base.device, 1, &m_render_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_base.device, 1, &m_render_finished_f);
	vkResetEvent(m_base.device, m_water_tex_ready_e);

	//manage terrain request
	if (m_terrain_valid)
	{
		terrain_manager::instance()->poll_results();
		vkWaitForFences(m_base.device, 1, &m_compute_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(m_base.device, 1, &m_compute_finished_f);
		terrain_manager::instance()->poll_requests();

		std::array<VkCommandBuffer, 2 > cbs = { m_terrain_request_cb, m_water_fft_cb };
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = cbs.size();
		submit.pCommandBuffers = cbs.data();

		std::lock_guard<std::mutex> lock(m_base.compute_queue_mutex);
		vkQueueSubmit(m_base.compute_queue, 1, &submit, m_compute_finished_f);
	}

	//record primary cb
	{
		auto cb = m_render_cb;
		//vkResetCommandBuffer(cb, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

		VkCommandBufferBeginInfo cb_begin = {};
		cb_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cb_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		assert(vkBeginCommandBuffer(cb, &cb_begin) == VK_SUCCESS);

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
		/*if(!m_renderables[RENDERABLE_TYPE_MAT_EM].empty() && !m_renderables[RENDERABLE_TYPE_SKYBOX].empty())
		{
			using namespace render_pass_environment_map_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.environment_map_gen;
			begin.renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];

			std::array<VkClearValue, ATT_COUNT> clears = {};
			clears[ATT_DEPTH].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT_COUNT;
			begin.pClearValues = clears.data();
			begin.renderArea.extent.width = ENVIRONMENT_MAP_SIZE;
			begin.renderArea.extent.height = ENVIRONMENT_MAP_SIZE;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			//vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_MAT_EM]);
			//vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_SKYBOX_EM]);
			//vkCmdEndRenderPass(cb);

			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image;
			b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 6;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}*/

		//dir shadow map gen
		if (m_opaque_objects.size()!=0)
		{
			using namespace render_pass_dir_shadow_map_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.dir_shadow_map_gen;
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

			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image;
			b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = FRUSTUM_SPLIT_COUNT;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//gbuffer assembler
		{
			using namespace render_pass_gbuffer_assembler;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.gbuffer_assembler;
			begin.renderPass = m_passes[RENDER_PASS_GBUFFER_ASSEMBLER];

			std::array<VkClearValue, ATT_COUNT> clears = {};
			clears[ATT_DEPTHSTENCIL].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT_COUNT;
			begin.pClearValues = clears.data();
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			if(m_opaque_objects.size()!=0)
				vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_MAT_OPAQUE]);

			if (m_terrain_valid)
			{
				vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_TERRAIN_DRAWER]);
			}

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SS_DIR_SHADOW_MAP_GEN].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdEndRenderPass(cb);
		}

		//barrier for ssds map
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image;
			b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}
		//barrier for pos and normal images
		{
			VkImageMemoryBarrier bs[2] = {};
			for (auto& b : bs)
			{
				b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				b.subresourceRange.baseArrayLayer = 0;
				b.subresourceRange.baseMipLevel = 0;
				b.subresourceRange.layerCount = 1;
				b.subresourceRange.levelCount = 1;
			}
			bs[0].image = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image;
			bs[1].image = m_res_image[RES_IMAGE_GB_NORMAL_AO].image;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 2, bs);
		}

		//barrier for basecolor_ssao and metalness_ssds
		{
			std::array<VkImageMemoryBarrier, 2> bs = {};
			for (auto& b : bs)
			{
				b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				b.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
				b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				b.subresourceRange.baseArrayLayer = 0;
				b.subresourceRange.baseMipLevel = 0;
				b.subresourceRange.layerCount = 1;
				b.subresourceRange.levelCount = 1;
			}
			bs[0].image = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image;
			bs[1].image = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0, 0, nullptr, 0, nullptr, 2, bs.data());
		}

		//barrier for depthstencil
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_GB_DEPTH].image;
			b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//ssao map gen
		{
			using namespace render_pass_ssao_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.ssao_map_gen;
			begin.renderPass = m_passes[RENDER_PASS_SSAO_MAP_GEN];
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SSAO_GEN].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);
			vkCmdEndRenderPass(cb);
		}

		//barrier for ssao map
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;
			b.image = m_res_image[RES_IMAGE_SSAO_MAP].image;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//preimage assembler
		{
			using namespace render_pass_preimage_assembler;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.preimage_assembler;
			begin.renderPass = m_passes[RENDER_PASS_PREIMAGE_ASSEMBLER];
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SSAO_BLUR].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SSR_RAY_CASTING].bind(cb);
			vkCmdDraw(cb, m_base.swap_chain_image_extent.width, m_base.swap_chain_image_extent.height, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_IMAGE_ASSEMBLER].bind(cb);
			if (m_sky_valid)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_IMAGE_ASSEMBLER].pl,
					1, 1, &m_sky.ds, 0, nullptr);
			}
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SKY_DRAWER].bind(cb);
			if (m_sky_valid)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_SKY_DRAWER].pl,
					1, 1, &m_sky.ds, 0, nullptr);
				vkCmdDraw(cb, 1, 1, 0, 0);
			}

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SUN_DRAWER].bind(cb);
			if (m_sky_valid)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_SUN_DRAWER].pl,
					1, 1, &m_sky.ds, 0, nullptr);
				vkCmdDraw(cb, 18, 1, 0, 0);
			}

			vkCmdEndRenderPass(cb);
		}

		/*//barrier for preimage
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_PREIMAGE].image;
			b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}*/

		//refraction map gen
		{
			using namespace render_pass_refraction_image_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.clearValueCount = ATT_COUNT;
			std::array<VkClearValue, ATT_COUNT> clears;
			clears[ATT_REFRACTION_IMAGE].color = { 0.f, 0.f, 0.f, 0.f };
			begin.pClearValues = clears.data();
			begin.framebuffer = m_fbs.refraction_image_gen;
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };
			begin.renderPass = m_passes[RENDER_PASS_REFRACTION_IMAGE_GEN];

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_REFRACTION_IMAGE_GEN].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);
			vkCmdEndRenderPass(cb);
		}

		//barrier for refraction image
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_REFRACTION_IMAGE].image;
			b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//water
		{
			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.water;
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };
			begin.renderPass = m_passes[RENDER_PASS_WATER];

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_INLINE);
			
			m_gps[GP_WATER_DRAWER].bind(cb);

			if (m_water_valid)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_WATER_DRAWER].pl,
					1, 1, &m_water.ds, 0, nullptr);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_WATER_DRAWER].pl,
					2, 1, &m_sky.ds, 0, nullptr);

				vkCmdDraw(cb, m_water_tiles_count.x * 4, m_water_tiles_count.y, 0, 0);
			}

			vkCmdEndRenderPass(cb);
		}

		/*//barrier for preimage
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_PREIMAGE].image;
			b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}*/

		//transition bloom blur image
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_BLOOM_BLUR].image;
			b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			b.srcAccessMask = 0;
			b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 2;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//blit preimage to bloom blur
		{
			VkImageBlit blit = {};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { 
				static_cast<int32_t>(m_base.swap_chain_image_extent.width),
				static_cast<int32_t>(m_base.swap_chain_image_extent.height), 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.srcSubresource.mipLevel = 0;

			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { 
				static_cast<int32_t>(m_base.swap_chain_image_extent.width / BLOOM_IMAGE_SIZE_FACTOR),
				static_cast<int32_t>(m_base.swap_chain_image_extent.height / BLOOM_IMAGE_SIZE_FACTOR), 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;
			blit.dstSubresource.mipLevel = 0;

			vkCmdBlitImage(cb, m_res_image[RES_IMAGE_PREIMAGE].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				m_res_image[RES_IMAGE_BLOOM_BLUR].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
		}

		//transition bloom blur
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_BLOOM_BLUR].image;
			b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 2;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//barrier for preimage
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_PREIMAGE].image;
			b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}


		assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
	}

	t.stop();
	//std::cout << "render_function_time: " << t.get() << std::endl;

	uint32_t image_index;
	vkAcquireNextImageKHR(m_base.device, m_base.swap_chain, std::numeric_limits<uint64_t>::max(), m_image_available_s,
		VK_NULL_HANDLE, &image_index);

	//submit render_buffer
	{
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_render_cb;
		std::array<VkSemaphore, 2> signal_s = { m_render_finished_s, m_preimage_ready_s };
		submit.pSignalSemaphores = signal_s.data();
		submit.signalSemaphoreCount = 2;
		
		vkWaitForFences(m_base.device, 1, &m_compute_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
		//vkSetEvent(m_base.device, m_water_tex_ready_e);
		
		std::lock_guard<std::mutex> lock(m_base.graphics_queue_mutex);
		assert(vkQueueSubmit(m_base.graphics_queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
	}

	//submit bloom blur
	{
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_bloom_blur_cb;
		submit.pSignalSemaphores = &m_bloom_blur_ready_s;
		submit.signalSemaphoreCount = 1;
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &m_preimage_ready_s;
		VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		submit.pWaitDstStageMask = &wait;

		std::lock_guard<std::mutex> lock(m_base.compute_queue_mutex);
		assert(vkQueueSubmit(m_base.compute_queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
	}

	//submit postprocessing
	{
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_present_cbs[image_index];

		std::array<VkSemaphore, 3> wait_ss = { m_image_available_s, m_render_finished_s, m_bloom_blur_ready_s };
		submit.pWaitSemaphores = wait_ss.data();
		submit.waitSemaphoreCount = wait_ss.size();
		std::array<VkPipelineStageFlags, 3> waits = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		submit.pWaitDstStageMask = waits.data();
		submit.pSignalSemaphores = &m_present_ready_ss[image_index];
		submit.signalSemaphoreCount = 1;

		std::lock_guard<std::mutex> lock(m_base.graphics_queue_mutex);
		assert(vkQueueSubmit(m_base.graphics_queue, 1, &submit, m_render_finished_f) == VK_SUCCESS);
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
		
		assert(vkQueuePresentKHR(m_base.present_queue, &present) == VK_SUCCESS);
	}

}

void gta5_pass::process_settings()
{
	auto projs = calc_projs();
	float height_bias = 1000.f;
	float height = height_bias+m_render_settings.pos.y;

	//environment map gen mat
	{
		auto data = m_res_data.get<RES_DATA_ENVIRONMENT_MAP_GEN_MAT>();
		data->ambient_irradiance = m_render_settings.ambient_irradiance;
		data->dir = m_render_settings.light_dir;
		data->irradiance = m_render_settings.irradiance;
		data->view = m_render_settings.view;
	}

	//environment map gen skybox
	{
		auto data = m_res_data.get<RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX>();
		data->view = m_render_settings.view;
	}

	//dir shadow map gen
	{
		auto data = m_res_data.get<RES_DATA_DIR_SHADOW_MAP_GEN>();
		auto projs = calc_projs();
		std::array<glm::mat4, projs.size()> projs_from_world;
		for (uint32_t i = 0; i < projs.size(); ++i)
			projs_from_world[i] = projs[i] * m_render_settings.view;
		memcpy(data->projs, projs_from_world.data(), sizeof(glm::mat4)*projs.size());
	}

	//gbuffer gen
	{
		auto data = m_res_data.get<RES_DATA_GBUFFER_GEN>();
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->cam_pos = m_render_settings.pos;
	}

	//ss dir shadow map gen
	{
		auto data = m_res_data.get<RES_DATA_SS_DIR_SHADOW_MAP_GEN>();
		data->light_dir = m_render_settings.light_dir;
		data->view = m_render_settings.view;
		data->far = m_render_settings.far;
		data->near = m_render_settings.near;
		memcpy(data->projs, projs.data(), sizeof(glm::mat4)*projs.size());
	}

	//image assembler data
	{
		auto data = m_res_data.get<RES_DATA_IMAGE_ASSEMBLER>();
		data->ambient_irradiance = m_render_settings.ambient_irradiance;
		data->dir = m_render_settings.light_dir;
		data->irradiance = m_render_settings.irradiance;
		data->cam_pos = m_render_settings.pos;
		data->height_bias = height_bias;
		data->previous_proj_x_view = m_previous_proj_x_view;
		data->previous_view_pos = m_previous_view_pos;
	}

	glm::mat4 view_at_origin = m_render_settings.view;
	view_at_origin[3][0] = 0.f;
	view_at_origin[3][1] = 0.f;
	view_at_origin[3][2] = 0.f;
	glm::mat4 proj_x_view_at_origin = m_render_settings.proj*view_at_origin;

	//sky drawer data
	{
		auto data = m_res_data.get<RES_DATA_SKY_DRAWER>();
		
		data->proj_x_view_at_origin = proj_x_view_at_origin;
		data->height = height;
		data->irradiance = m_render_settings.irradiance;
		data->light_dir = m_render_settings.light_dir;
	}

	//sun drawer data
	{
		auto data = m_res_data.get<RES_DATA_SUN_DRAWER>();
		data->height = height;
		data->irradiance = m_render_settings.irradiance;
		data->light_dir = m_render_settings.light_dir;
		data->proj_x_view_at_origin = proj_x_view_at_origin;
		data->helper_dir = get_orthonormal(m_render_settings.light_dir);
	}

	//terrain drawer data
	{
		auto data = m_res_data.get<RES_DATA_TERRAIN_DRAWER>();
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->view_pos = m_render_settings.pos;
	}

	//terrain tile request data
	{
		auto data = m_res_data.get<RES_DATA_TERRAIN_TILE_REQUEST>();
		data->far = m_render_settings.far;
		data->near = m_render_settings.near;
		data->view_pos = m_render_settings.pos;
	}

	//water drawer data
	{
		auto data = m_res_data.get<RES_DATA_WATER_DRAWER>();
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->half_resolution.x = static_cast<float>(m_base.swap_chain_image_extent.width) / 2.f;
		data->half_resolution.y = static_cast<float>(m_base.swap_chain_image_extent.height) / 2.f;
		data->light_dir = m_render_settings.light_dir;
		data->view_pos = m_render_settings.pos;
		data->tile_size_in_meter = m_water.grid_size_in_meters;
		data->tile_offset = (glm::floor(glm::vec2(m_render_settings.pos.x - m_render_settings.far, m_render_settings.pos.z - m_render_settings.far)
			/ m_water.grid_size_in_meters)+glm::vec2(0.5f))*
			m_water.grid_size_in_meters;
		m_water_tiles_count = static_cast<glm::uvec2>(glm::ceil(glm::vec2(2.f*m_render_settings.far) / m_water.grid_size_in_meters));
		data->ambient_irradiance = m_render_settings.ambient_irradiance;
		data->height_bias = height_bias;
		data->mirrored_proj_x_view = glm::mat4(1.f); //CORRECT IT!!!
		data->irradiance = m_render_settings.irradiance;
	}

	//refraction map gen
	{
		auto data = m_res_data.get<RES_DATA_REFRACTION_MAP_GEN>();
		data->far = m_render_settings.far;
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->view_pos_at_ground = { m_render_settings.pos.x, glm::min(0.05f*glm::dot(m_render_settings.wind, m_render_settings.wind) / 9.81f,
			m_render_settings.pos.y - m_render_settings.near - 0.000001f), m_render_settings.pos.z };
	}

	//ssr ray casting
	{
		auto data = m_res_data.get<RES_DATA_SSR_RAY_CASTING>();
		data->proj_x_view= m_render_settings.proj*m_render_settings.view;
		data->ray_length = glm::length(glm::vec3(m_render_settings.far / m_render_settings.proj[0][0], m_render_settings.far /
			m_render_settings.proj[1][1], m_render_settings.far));
		data->view_pos = m_render_settings.pos;
	}

	//water compute data
	{
		auto data = m_res_data.get<RES_DATA_WATER_COMPUTE>();
		data->one_over_wind_speed_to_the_4 = 1.f / powf(glm::length(m_render_settings.wind), 4.f);
		data->wind_dir = glm::normalize(m_render_settings.wind);
		data->time = m_render_settings.time;
	}

	m_previous_proj_x_view = m_render_settings.proj*m_render_settings.view;
	m_previous_view_pos = m_render_settings.pos;
}

std::array<glm::mat4, FRUSTUM_SPLIT_COUNT> gta5_pass::calc_projs()
{
	std::array<glm::mat4, FRUSTUM_SPLIT_COUNT> projs;

	glm::vec3 light_dir = static_cast<glm::mat3>(m_render_settings.view)*m_render_settings.light_dir;
	glm::mat4 light = glm::lookAtLH(glm::vec3(0.f), light_dir, glm::vec3(0.f, 1.f, 0.f));
	std::array<std::array<glm::vec3, 4>, FRUSTUM_SPLIT_COUNT + 1> frustum_points;
	
	glm::vec2 max[2];
	glm::vec2 min[2];

	for (uint32_t i = 0; i < FRUSTUM_SPLIT_COUNT + 1; ++i)
	{
		float z = m_render_settings.near + std::powf(static_cast<float>(i) / static_cast<float>(FRUSTUM_SPLIT_COUNT), 3.f)*
			(m_render_settings.far - m_render_settings.near);		
		z *= (-1.f);

		std::array<float, 4> x = { -1.f, 1.f, -1.f, 1.f };
		std::array<float, 4> y = { -1.f, -1.f, 1.f, 1.f };

		max[i % 2] = glm::vec2(std::numeric_limits<float>::min());
		min[i % 2] = glm::vec2(std::numeric_limits<float>::max());

		for (uint32_t j=0; j<4; ++j)
		{
			glm::vec3 v = { x[j]*z / m_render_settings.proj[0][0], y[j]*z / m_render_settings.proj[1][1], z };
			v = static_cast<glm::mat3>(light)*v;
			frustum_points[i][j] = v;

			max[i % 2] = componentwise_max(max[i % 2], { v.x, v.y });
			min[i % 2] = componentwise_min(min[i % 2], { v.x, v.y });
		}

		if (i > 0)
		{
			glm::vec2 frustum_max = componentwise_max(max[0], max[1]);
			glm::vec2 frustum_min = componentwise_min(min[0], min[1]);

			glm::vec2 center = (frustum_max + frustum_min)*0.5f;
			glm::vec2 scale = glm::vec2(2.f)/(frustum_max - frustum_min);

			glm::mat4 proj(1.f);
			proj[0][0] = scale.x;
			proj[1][1] = scale.y;
			proj[2][2] = 0.5f / 1000.f;
			proj[3][0] = -scale.x*center.x;
			proj[3][1] = -scale.y*center.y;
			proj[3][2] = 0.5f;

			projs[i - 1] = proj*light;
		}
	}
	return projs; //from view space
}

void gta5_pass::set_terrain(base_resource* terrain)
{
	auto t = reinterpret_cast<resource<RES_TYPE_TERRAIN>*>(terrain->data);

	m_terrain.ds = t->ds;
	m_terrain.request_ds = t->request_ds;
	terrain_manager::instance()->set_terrain(t);

	//record terrain request cb
	{
		auto cb = m_terrain_request_cb;
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = 0;

		assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

		m_cps[CP_TERRAIN_TILE_REQUEST].bind(cb, VK_PIPELINE_BIND_POINT_COMPUTE);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_cps[CP_TERRAIN_TILE_REQUEST].pl,
			1, 1, &m_terrain.request_ds, 0, nullptr);

		glm::uvec2 group_count = t->tile_count;
		vkCmdDispatch(cb, group_count.x, group_count.y, 1);

		assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
	}

	//record terrain drawer cb
	{
		VkCommandBufferInheritanceInfo inheritance = {};
		inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritance.framebuffer = m_fbs.gbuffer_assembler;
		inheritance.occlusionQueryEnable = VK_FALSE;
		inheritance.renderPass = m_passes[RENDER_PASS_GBUFFER_ASSEMBLER];
		inheritance.subpass = render_pass_gbuffer_assembler::SUBPASS_GBUFFER_GEN;

		auto cb = m_secondary_cbs[SECONDARY_CB_TERRAIN_DRAWER];
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.pInheritanceInfo = &inheritance;
		begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);
		m_gps[GP_TERRAIN_DRAWER].bind(cb);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_TERRAIN_DRAWER].pl,
			1, 1, &m_terrain.ds, 0, nullptr);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_TERRAIN_DRAWER].pl,
			2, 4, m_terrain.opaque_material_dss.data(),
			0, nullptr);
		vkCmdDraw(cb, 4 * t->tile_count.x,
			t->tile_count.y, 0, 0);

		assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
	}

	m_terrain_valid = true;
}

void gta5_pass::set_water(base_resource* water)
{
	auto w = reinterpret_cast<resource<RES_TYPE_WATER>*>(water->data);
	m_water.ds = w->ds;
	m_water.fft_ds = w->fft_ds;
	m_water.grid_size_in_meters = w->grid_size_in_meters;

	//record water fft cb
	auto cb = m_water_fft_cb;

	VkCommandBufferBeginInfo b = {};
	b.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	
	assert(vkBeginCommandBuffer(cb, &b) == VK_SUCCESS);

	//acquire barrier
	{
		VkImageMemoryBarrier b = {};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.image = w->tex.image;
		b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 2;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
	}

	m_cps[CP_WATER_FFT].bind(cb, VK_PIPELINE_BIND_POINT_COMPUTE);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_cps[CP_WATER_FFT].pl,
		1, 1, &w->fft_ds, 0, nullptr);
	
	uint32_t fft_axis = 0;
	vkCmdPushConstants(cb, m_cps[CP_WATER_FFT].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &fft_axis);
	vkCmdDispatch(cb, 1, 1024, 1);

	//memory barrier
	{
		VkImageMemoryBarrier b = {};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		b.image = w->tex.image;
		b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.layerCount = 2;
		b.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &b);
	}	

	fft_axis = 1;
	vkCmdPushConstants(cb, m_cps[CP_WATER_FFT].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &fft_axis);
	vkCmdDispatch(cb, 1, 1024, 1);

	//release barrier
	{
		VkImageMemoryBarrier b = {};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.image = w->tex.image;
		b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 2;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &b);
	}

	assert(vkEndCommandBuffer(cb) == VK_SUCCESS);

	bool m_water_valid = true;
}

void gta5_pass::wait_for_finish()
{
	vkWaitForFences(m_base.device, 1, &m_render_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
}