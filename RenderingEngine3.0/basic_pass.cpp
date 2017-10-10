#include "basic_pass.h"

#include "resource_manager.h"

using namespace rcq;

basic_pass* basic_pass::m_instance = nullptr;

basic_pass::basic_pass(const base_info& info, const renderable_container& renderables):
	m_base(info), m_renderables(renderables)
{
	create_render_pass();
	create_command_pool();
	create_resources();
	create_descriptors();
	create_opaque_mat_pipeline();
	create_omni_light_pipeline();
	create_skybox_pipeline();
	create_framebuffers();
	allocate_command_buffers();
	create_semaphores();
	create_fences();

	m_record_masks.resize(m_fbs.size());
	for (auto& bs : m_record_masks)
		bs.reset();
}


basic_pass::~basic_pass()
{
	wait_for_finish();

	for (auto f : m_primary_cb_finished_fs)
		vkDestroyFence(m_base.device, f, host_memory_manager);

	vkDestroySemaphore(m_base.device, m_render_finished_s, host_memory_manager);
	vkDestroySemaphore(m_base.device, m_render_finished_s2, host_memory_manager);
	vkDestroySemaphore(m_base.device, m_image_available_s, host_memory_manager);

	for (auto& dsl : m_per_frame_dsls)
		vkDestroyDescriptorSetLayout(m_base.device, dsl, host_memory_manager);
	vkDestroyDescriptorSetLayout(m_base.device, m_gbuffer_dsl, host_memory_manager);
	vkDestroyDescriptorPool(m_base.device, m_dp, host_memory_manager);
	
	vkUnmapMemory(m_base.device, m_per_frame_b_mem);
	vkDestroyBuffer(m_base.device, m_per_frame_b, host_memory_manager);

	for (auto& fb : m_fbs)
		vkDestroyFramebuffer(m_base.device, fb, host_memory_manager);

	vkDestroyCommandPool(m_base.device, m_cp, host_memory_manager);

	vkDestroyImageView(m_base.device, m_depth_tex.view, host_memory_manager);
	vkDestroyImage(m_base.device, m_depth_tex.image, host_memory_manager);
	for (auto& view : m_gbuffer_views)
		vkDestroyImageView(m_base.device, view, host_memory_manager);
	vkDestroyImage(m_base.device, m_gbuffer_image, host_memory_manager);

	auto destroy=std::make_unique<destroy_package>();
	destroy->ids[RESOURCE_TYPE_MEMORY].push_back(RENDER_PASS_BASIC);
	resource_manager::instance()->push_destroy_package(std::move(destroy));

	for (auto& pl : m_pls)
		vkDestroyPipelineLayout(m_base.device, pl, host_memory_manager);
	for (auto& gp : m_gps)
		vkDestroyPipeline(m_base.device, gp, host_memory_manager);

	vkDestroyRenderPass(m_base.device, m_pass, host_memory_manager);
}

void basic_pass::init(const base_info& info, const renderable_container& renderables)
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("core is already initialised!");
	}
	m_instance = new basic_pass(info, renderables);
}

void basic_pass::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy core, it doesn't exist!");
	}

	delete m_instance;
}

void basic_pass::create_render_pass()
{
	VkAttachmentDescription color_out{}, depth_stencil{}, gbuffer{};

	color_out.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	color_out.format = m_base.swap_chain_image_format;
	color_out.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_out.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_out.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_out.samples = VK_SAMPLE_COUNT_1_BIT;
	color_out.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_out.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
	
	depth_stencil.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_stencil.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_stencil.format = find_depth_format(m_base.physical_device);
	depth_stencil.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_stencil.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_stencil.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_stencil.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_stencil.samples = VK_SAMPLE_COUNT_1_BIT;

	gbuffer.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gbuffer.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gbuffer.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	gbuffer.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	gbuffer.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	gbuffer.samples = VK_SAMPLE_COUNT_1_BIT;
	gbuffer.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	gbuffer.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkAttachmentDescription attachments[] = { depth_stencil, gbuffer, gbuffer, gbuffer, gbuffer, gbuffer, color_out };
	
	VkAttachmentReference depth_ref = {};
	depth_ref.attachment = 0;
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//mat pass refs

	VkAttachmentReference gbuffer_mat_refs[5];
	for (int i = 0; i < 5; ++i)
	{
		gbuffer_mat_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		gbuffer_mat_refs[i].attachment = i + 1;
	}

	//material subpass
	VkSubpassDescription material_subpass = {};
	material_subpass.colorAttachmentCount = 5;
	material_subpass.pColorAttachments = gbuffer_mat_refs;
	material_subpass.pDepthStencilAttachment = &depth_ref;
	material_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//light pass refs
	VkAttachmentReference gbuffer_light_refs[5];
	for (int i = 0; i < 5; ++i)
	{
		gbuffer_light_refs[i].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbuffer_light_refs[i].attachment = i + 1;
	}

	VkAttachmentReference color_out_ref;
	color_out_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color_out_ref.attachment = 6;


	//light subpass
	VkSubpassDescription light_subpass = {};
	light_subpass.colorAttachmentCount = 1;
	light_subpass.inputAttachmentCount = 5;
	light_subpass.pColorAttachments = &color_out_ref;
	light_subpass.pDepthStencilAttachment = &depth_ref;
	light_subpass.pInputAttachments = gbuffer_light_refs;
	light_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//skybox subpass
	VkSubpassDescription skybox_subpass = {};
	skybox_subpass.colorAttachmentCount = 1;
	skybox_subpass.pColorAttachments = &color_out_ref;
	skybox_subpass.pDepthStencilAttachment = &depth_ref;
	skybox_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	
	VkSubpassDescription subpasses[] = { material_subpass, light_subpass, skybox_subpass };

	//dependencies
	VkSubpassDependency ext_dependency = {};
	ext_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	ext_dependency.dstSubpass = SUBPASS_MAT;
	ext_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	ext_dependency.srcAccessMask = 0;
	ext_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	ext_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency mat_light_dependency = {};
	mat_light_dependency.srcSubpass = SUBPASS_MAT;
	mat_light_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	mat_light_dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	mat_light_dependency.dstSubpass = SUBPASS_LIGHT;
	mat_light_dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	mat_light_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

	VkSubpassDependency light_skybox_dependency = {};
	light_skybox_dependency.srcSubpass = SUBPASS_LIGHT;
	light_skybox_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	light_skybox_dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	light_skybox_dependency.dstSubpass = SUBPASS_SKYBOX;
	light_skybox_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	light_skybox_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency dependencies[3] = { ext_dependency, mat_light_dependency, light_skybox_dependency };

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 7;
	info.dependencyCount = 3;
	info.pAttachments = attachments;
	info.pDependencies = dependencies;
	info.pSubpasses = subpasses;
	info.subpassCount = 3;

	if (vkCreateRenderPass(m_base.device, &info, host_memory_manager, &m_pass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

void basic_pass::create_omni_light_pipeline()
{
	auto vert_code = read_file("shaders/basic_pass/light_subpass/omni_light/vert.spv");
	auto frag_code = read_file("shaders/basic_pass/light_subpass/omni_light/frag.spv");

	auto vert_module = create_shader_module(m_base.device, vert_code);
	auto frag_module = create_shader_module(m_base.device, frag_code);

	VkPipelineShaderStageCreateInfo shaders[2] = {};
	shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[0].module = vert_module;
	shaders[0].pName = "main";
	shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

	shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[1].module = frag_module;
	shaders[1].pName = "main";
	shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	
	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.pVertexAttributeDescriptions = nullptr;
	vertex_input.pVertexBindingDescriptions = 0;
	vertex_input.vertexAttributeDescriptionCount = 0;
	vertex_input.vertexBindingDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo assembly = {};
	assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.primitiveRestartEnable = VK_FALSE;
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
	
	VkViewport vp = {};
	vp.height = (float)m_base.swap_chain_image_extent.height;
	vp.width = (float)m_base.swap_chain_image_extent.width;
	vp.maxDepth = 1.f;
	vp.minDepth = 0.f;

	VkRect2D scissor = {};
	scissor.extent = m_base.swap_chain_image_extent;
	scissor.offset = { 0,0 };

	VkPipelineViewportStateCreateInfo viewport = {};
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.pScissors = &scissor;
	viewport.pViewports = &vp;
	viewport.scissorCount = 1;
	viewport.viewportCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.lineWidth = 1.f;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depth = {};
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth.depthBoundsTestEnable = VK_TRUE;
	depth.minDepthBounds = 0.f;
	depth.maxDepthBounds = 0.9999999999999f;
	depth.depthTestEnable = VK_FALSE;
	depth.depthWriteEnable = VK_FALSE;
	depth.stencilTestEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisample = {};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.alphaToCoverageEnable = VK_FALSE;
	multisample.alphaToOneEnable = VK_FALSE;
	multisample.minSampleShading = 0.f;
	multisample.sampleShadingEnable = VK_FALSE;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;

	VkPipelineColorBlendStateCreateInfo color_blend = {};
	color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend.attachmentCount = 1;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.pAttachments = &color_blend_attachment;

	VkDescriptorSetLayout dsls[2] = { resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI), m_gbuffer_dsl };
	VkPipelineLayoutCreateInfo layout = {};
	layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout.setLayoutCount = 2;
	layout.pSetLayouts = dsls;

	if (vkCreatePipelineLayout(m_base.device, &layout, host_memory_manager, &m_pls[RENDERABLE_TYPE_LIGHT_OMNI]) != VK_SUCCESS)
		throw std::runtime_error("failed to create light pipeline layout!");

	VkGraphicsPipelineCreateInfo gp_info = {};
	gp_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gp_info.basePipelineHandle = VK_NULL_HANDLE;
	gp_info.basePipelineIndex = -1;
	gp_info.layout = m_pls[RENDERABLE_TYPE_LIGHT_OMNI];
	gp_info.pColorBlendState = &color_blend;
	gp_info.pDepthStencilState = &depth;
	gp_info.pInputAssemblyState = &assembly;
	gp_info.pMultisampleState = &multisample;
	gp_info.pRasterizationState = &rasterizer;
	gp_info.pStages = shaders;
	gp_info.pVertexInputState = &vertex_input;
	gp_info.pViewportState = &viewport;
	gp_info.renderPass = m_pass;
	gp_info.stageCount = 2;
	gp_info.subpass = 1;

	if (vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, 1, &gp_info, host_memory_manager, &m_gps[RENDERABLE_TYPE_LIGHT_OMNI])
		!= VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline!");

	vkDestroyShaderModule(m_base.device, vert_module, host_memory_manager);
	vkDestroyShaderModule(m_base.device, frag_module, host_memory_manager);
}

void basic_pass::create_opaque_mat_pipeline()
{
	auto vert_code = read_file("shaders/basic_pass/mat_subpass/opaque_mat/vert.spv");
	auto frag_code = read_file("shaders/basic_pass/mat_subpass/opaque_mat/frag.spv");

	auto vert_module = create_shader_module(m_base.device, vert_code);
	auto frag_module = create_shader_module(m_base.device, frag_code);

	VkPipelineShaderStageCreateInfo shaders[2] = {};
	shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[0].module = vert_module;
	shaders[0].pName = "main";
	shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

	shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[1].module = frag_module;
	shaders[1].pName = "main";
	shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	constexpr auto attribs = get_vertex_input_attribute_descriptions();
	constexpr auto binding = get_vertex_input_binding_descriptions();
	vertex_input.pVertexAttributeDescriptions = attribs.data();
	vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribs.size());
	vertex_input.pVertexBindingDescriptions = binding.data();
	vertex_input.vertexBindingDescriptionCount = static_cast<uint32_t>(binding.size());
	
	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.primitiveRestartEnable = VK_FALSE;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	
	VkViewport vp = {};
	vp.height = (float)m_base.swap_chain_image_extent.height;
	vp.width = (float)m_base.swap_chain_image_extent.width;
	vp.minDepth = 0.f;
	vp.maxDepth = 1.f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = m_base.swap_chain_image_extent;

	VkPipelineViewportStateCreateInfo viewport = {};
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.pScissors = &scissor;
	viewport.pViewports = &vp;
	viewport.scissorCount = 1;
	viewport.viewportCount = 1;
	
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.lineWidth = 1.f;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisample = {};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.alphaToCoverageEnable = VK_FALSE;
	multisample.alphaToOneEnable = VK_FALSE;
	multisample.minSampleShading = 0.f;
	multisample.sampleShadingEnable = VK_FALSE;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthstencil = {};
	depthstencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthstencil.depthBoundsTestEnable = VK_FALSE;
	depthstencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthstencil.depthTestEnable = VK_TRUE;
	depthstencil.depthWriteEnable = VK_TRUE;
	depthstencil.maxDepthBounds = 1.f;
	depthstencil.minDepthBounds = 0.f;
	depthstencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.colorWriteMask= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendAttachmentState color_blend_attachments[5] = {
		color_blend_attachment,
		color_blend_attachment,
		color_blend_attachment,
		color_blend_attachment,
		color_blend_attachment
	};

	VkPipelineColorBlendStateCreateInfo color_blend = {};
	color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend.attachmentCount = 5;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.pAttachments=color_blend_attachments;

	VkPipelineLayoutCreateInfo layout = {};
	layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	VkDescriptorSetLayout dsls[] = { 
		m_per_frame_dsls[RENDERABLE_TYPE_MAT_OPAQUE], 
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_TR), 
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE) };
	layout.pSetLayouts = dsls;
	layout.setLayoutCount = 3;
	
	if (vkCreatePipelineLayout(m_base.device, &layout, host_memory_manager, &m_pls[RENDERABLE_TYPE_MAT_OPAQUE]) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!");

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.basePipelineHandle = VK_NULL_HANDLE;
	info.basePipelineIndex = -1;
	info.layout = m_pls[RENDERABLE_TYPE_MAT_OPAQUE];
	info.pColorBlendState = &color_blend;
	info.pDepthStencilState = &depthstencil;
	info.pInputAssemblyState = &input_assembly;
	info.pMultisampleState = &multisample;
	info.pRasterizationState = &rasterizer;
	info.pStages = shaders;
	info.pVertexInputState = &vertex_input;
	info.pViewportState = &viewport;
	info.renderPass = m_pass;
	info.subpass = 0;
	info.stageCount = 2;

	if (vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, 1, &info, host_memory_manager, &m_gps[RENDERABLE_TYPE_MAT_OPAQUE]) != VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline!");

	vkDestroyShaderModule(m_base.device, vert_module, host_memory_manager);
	vkDestroyShaderModule(m_base.device, frag_module, host_memory_manager);
}

void basic_pass::create_skybox_pipeline()
{
	auto vert_code = read_file("shaders/basic_pass/after_light_subpass/skybox/vert.spv");
	auto geom_code = read_file("shaders/basic_pass/after_light_subpass/skybox/geom.spv");
	auto frag_code = read_file("shaders/basic_pass/after_light_subpass/skybox/frag.spv");

	VkShaderModule vert_module = create_shader_module(m_base.device, vert_code);
	VkShaderModule geom_module = create_shader_module(m_base.device, geom_code);
	VkShaderModule frag_module = create_shader_module(m_base.device, frag_code);

	VkPipelineShaderStageCreateInfo shaders[3] = {};
	shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[0].module = vert_module;
	shaders[0].pName = "main";
	shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[1].module = geom_module;
	shaders[1].pName = "main";
	shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
	shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[2].module = frag_module;
	shaders[2].pName = "main";
	shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.pVertexAttributeDescriptions = nullptr;
	vertex_input.pVertexBindingDescriptions = 0;
	vertex_input.vertexAttributeDescriptionCount = 0;
	vertex_input.vertexBindingDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo assembly = {};
	assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.primitiveRestartEnable = VK_TRUE;
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

	VkViewport vp = {};
	vp.height = (float)m_base.swap_chain_image_extent.height;
	vp.width = (float)m_base.swap_chain_image_extent.width;
	vp.minDepth = 0.f;
	vp.maxDepth = 1.f;

	VkRect2D scissor = {};
	scissor.extent = m_base.swap_chain_image_extent;
	scissor.offset = { 0,0 };

	VkPipelineViewportStateCreateInfo viewport = {};
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.pScissors = &scissor;
	viewport.pViewports = &vp;
	viewport.scissorCount = 1;
	viewport.viewportCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.lineWidth = 1.f;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depth = {};
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth.depthBoundsTestEnable = VK_FALSE;
	depth.depthCompareOp = VK_COMPARE_OP_EQUAL;
	depth.depthTestEnable = VK_TRUE;
	depth.depthWriteEnable = VK_FALSE;
	depth.stencilTestEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisample = {};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.alphaToCoverageEnable = VK_FALSE;
	multisample.alphaToOneEnable = VK_FALSE;
	multisample.minSampleShading = 0.f;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample.sampleShadingEnable = VK_FALSE;
	
	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo color_blend = {};
	color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend.attachmentCount = 1;
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.pAttachments = &color_blend_attachment;
	
	VkDescriptorSetLayout dsls[2] = { m_per_frame_dsls[RENDERABLE_TYPE_MAT_OPAQUE], resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX) };

	VkPipelineLayoutCreateInfo layout = {};
	layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout.pSetLayouts = dsls;
	layout.setLayoutCount = 2;

	if (vkCreatePipelineLayout(m_base.device, &layout, host_memory_manager, &m_pls[RENDERABLE_TYPE_SKYBOX]) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!");

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.basePipelineHandle = VK_NULL_HANDLE;
	info.basePipelineIndex = -1;
	info.layout = m_pls[RENDERABLE_TYPE_SKYBOX];
	info.pColorBlendState = &color_blend;
	info.pDepthStencilState = &depth;
	info.pMultisampleState = &multisample;
	info.pRasterizationState = &rasterizer;
	info.pStages = shaders;
	info.pInputAssemblyState = &assembly;
	info.pVertexInputState = &vertex_input;
	info.pViewportState = &viewport;
	info.renderPass = m_pass;
	info.stageCount = 3;
	info.subpass = SUBPASS_SKYBOX;

	if (vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, 1, &info, host_memory_manager, &m_gps[RENDERABLE_TYPE_SKYBOX]) != VK_SUCCESS)
		throw std::runtime_error("failed to create skybox pipeline!");	

	vkDestroyShaderModule(m_base.device, vert_module, host_memory_manager);
	vkDestroyShaderModule(m_base.device, geom_module, host_memory_manager);
	vkDestroyShaderModule(m_base.device, frag_module, host_memory_manager);
}

void basic_pass::create_resources()
{
	VkFormat depth_format = find_depth_format(m_base.physical_device);
	//create depth image
	VkImageCreateInfo depth_image_info = {};
	depth_image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depth_image_info.arrayLayers = 1;
	depth_image_info.extent.height = m_base.swap_chain_image_extent.height;
	depth_image_info.extent.width = m_base.swap_chain_image_extent.width;
	depth_image_info.extent.depth = 1;
	depth_image_info.format = depth_format;
	depth_image_info.imageType = VK_IMAGE_TYPE_2D;
	depth_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_image_info.mipLevels = 1;
	depth_image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	depth_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	depth_image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	if (vkCreateImage(m_base.device, &depth_image_info, host_memory_manager,
		&m_depth_tex.image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create depth image!");
	}

	//create gbuffer image
	VkImageCreateInfo gbuffer_image_info = {};
	gbuffer_image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	gbuffer_image_info.arrayLayers = 5;
	gbuffer_image_info.extent.width = m_base.swap_chain_image_extent.width;
	gbuffer_image_info.extent.height = m_base.swap_chain_image_extent.height;
	gbuffer_image_info.extent.depth = 1;
	gbuffer_image_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	gbuffer_image_info.imageType = VK_IMAGE_TYPE_2D;
	gbuffer_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gbuffer_image_info.mipLevels = 1;
	gbuffer_image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	gbuffer_image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	gbuffer_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	gbuffer_image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

	if (vkCreateImage(m_base.device, &gbuffer_image_info, host_memory_manager, &m_gbuffer_image) != VK_SUCCESS)
		throw std::runtime_error("failed to create gbuffer image!");

	//create per frame buffer
	VkBufferCreateInfo buffer_create_info = {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_create_info.size = sizeof(camera_data);
	buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (vkCreateBuffer(m_base.device, &buffer_create_info, host_memory_manager, &m_per_frame_b) != VK_SUCCESS)
		throw std::runtime_error("failed to create per frame buffer!");


	//send memory build package to resource manager
	VkMemoryRequirements depth_image_mr, gbuffer_image_mr, per_frame_buffer_mr;
	vkGetImageMemoryRequirements(m_base.device, m_depth_tex.image, &depth_image_mr);
	vkGetImageMemoryRequirements(m_base.device, m_gbuffer_image, &gbuffer_image_mr);
	vkGetBufferMemoryRequirements(m_base.device, m_per_frame_b, &per_frame_buffer_mr);

	std::vector<VkMemoryAllocateInfo> alloc_info(3);
	alloc_info[0].sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info[0].allocationSize = depth_image_mr.size;
	alloc_info[0].memoryTypeIndex = find_memory_type(m_base.physical_device, depth_image_mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	alloc_info[1].sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info[1].allocationSize = gbuffer_image_mr.size;
	alloc_info[1].memoryTypeIndex = find_memory_type(m_base.physical_device, gbuffer_image_mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	alloc_info[2].sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info[2].allocationSize = per_frame_buffer_mr.size;
	alloc_info[2].memoryTypeIndex = find_memory_type(m_base.physical_device, per_frame_buffer_mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	build_package build;
	std::get<RESOURCE_TYPE_MEMORY>(build).emplace_back(RENDER_PASS_BASIC, std::move(alloc_info));
	resource_manager::instance()->process_build_package(std::move(build));

	//bind memory to image and buffer and map
	memory mem = resource_manager::instance()->get<RESOURCE_TYPE_MEMORY>(RENDER_PASS_BASIC);
	vkBindImageMemory(m_base.device, m_depth_tex.image, mem[0], 0);
	vkBindImageMemory(m_base.device, m_gbuffer_image, mem[1], 0);
	vkBindBufferMemory(m_base.device, m_per_frame_b, mem[2], 0);
	m_per_frame_b_mem = mem[2];
	void* raw_data;
	vkMapMemory(m_base.device, m_per_frame_b_mem, 0, sizeof(camera_data), 0, &raw_data);
	m_per_frame_b_data = static_cast<char*>(raw_data);

	//create depth view
	VkImageViewCreateInfo view_create_info = {};
	view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_create_info.format = depth_format;
	view_create_info.image = m_depth_tex.image;
	view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view_create_info.subresourceRange.baseArrayLayer = 0;
	view_create_info.subresourceRange.baseMipLevel = 0;
	view_create_info.subresourceRange.layerCount = 1;
	view_create_info.subresourceRange.levelCount = 1;

	if (vkCreateImageView(m_base.device, &view_create_info, host_memory_manager,
		&m_depth_tex.view) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create depth image view!");
	}

	//create gbuffer views
	for (uint32_t i = 0; i < m_gbuffer_views.size(); ++i)
	{
		VkImageViewCreateInfo gbuffer_view_info = {};
		gbuffer_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		gbuffer_view_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		gbuffer_view_info.image = m_gbuffer_image;
		gbuffer_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		gbuffer_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		gbuffer_view_info.subresourceRange.baseArrayLayer = i;
		gbuffer_view_info.subresourceRange.baseMipLevel = 0;
		gbuffer_view_info.subresourceRange.layerCount = 1;
		gbuffer_view_info.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_base.device, &gbuffer_view_info, host_memory_manager, &m_gbuffer_views[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create gbuffer image views!");
	}

	//transition depth image to depth stencil attachment optimal layout, 
	//and  gbuffer image to color attachment optimal layout
	VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp);

	VkImageMemoryBarrier depth_barrier = {};
	depth_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	depth_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	depth_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	depth_barrier.image = m_depth_tex.image;
	depth_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (depth_format == VK_FORMAT_D24_UNORM_S8_UINT || depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT)
		depth_barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

	depth_barrier.subresourceRange.baseArrayLayer = 0;
	depth_barrier.subresourceRange.baseMipLevel = 0;
	depth_barrier.subresourceRange.layerCount = 1;
	depth_barrier.subresourceRange.levelCount = 1;

	depth_barrier.srcAccessMask = 0;
	depth_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	vkCmdPipelineBarrier(cb,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &depth_barrier
	);

	VkImageMemoryBarrier gbuffer_barrier = {};
	gbuffer_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	gbuffer_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	gbuffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	gbuffer_barrier.image = m_gbuffer_image;
	gbuffer_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gbuffer_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gbuffer_barrier.srcAccessMask = 0;
	gbuffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	gbuffer_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	gbuffer_barrier.subresourceRange.baseArrayLayer = 0;
	gbuffer_barrier.subresourceRange.baseMipLevel = 0;
	gbuffer_barrier.subresourceRange.layerCount = 5;
	gbuffer_barrier.subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr,
		0, nullptr, 1, &gbuffer_barrier);

	end_single_time_command_buffer(m_base.device, m_cp, m_base.graphics_queue, cb);
}

void rcq::basic_pass::create_command_pool()
{
	VkCommandPoolCreateInfo cp_info = {};
	cp_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cp_info.queueFamilyIndex = m_base.queue_families.graphics_family;
	cp_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(m_base.device, &cp_info, host_memory_manager, &m_cp) != VK_SUCCESS)
		throw std::runtime_error("failed to create command pool!");
}

void basic_pass::create_framebuffers()
{
	std::array<VkImageView, 7> attachments = {};
	attachments[0] = m_depth_tex.view;
	for (uint32_t i = 0; i < m_gbuffer_views.size(); ++i)
		attachments[i + 1] = m_gbuffer_views[i];

	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.attachmentCount = 7;
	fb_info.pAttachments = attachments.data();
	fb_info.layers = 1;
	fb_info.height = m_base.swap_chain_image_extent.height;
	fb_info.width = m_base.swap_chain_image_extent.width;
	fb_info.renderPass = m_pass;

	m_fbs.resize(m_base.swap_chain_image_views.size());
	for (size_t i = 0; i < m_fbs.size(); ++i)
	{
		attachments[6] = m_base.swap_chain_image_views[i];
		if (vkCreateFramebuffer(m_base.device, &fb_info, host_memory_manager, &m_fbs[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create framebuffer!");	
	}
}

VkSemaphore basic_pass::record_and_render(const glm::mat4& view, const std::optional<update_proj>& proj_info,
	std::bitset<RENDERABLE_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_mask, VkSemaphore wait_s)
{
	for (auto& rm : m_record_masks)
		rm |= record_mask;

	// update camera
	if (proj_info)
	{
		m_proj = proj_info.value();
		update_csm_data();
	}
	camera_data cam;
	cam.proj_x_view = m_proj.proj*view;
	cam.pos = { view[3][0], view[3][1], view[3][2] };
	memcpy(m_per_frame_b_data, &cam, sizeof(camera_data));

	//acquire swap chain image
	uint32_t image_index;

	if (vkAcquireNextImageKHR(m_base.device, m_base.swap_chain, std::numeric_limits<uint64_t>::max(), m_image_available_s,
		VK_NULL_HANDLE, &image_index) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	timer t;
	t.start();
	if (vkWaitForFences(m_base.device, 1, &m_primary_cb_finished_fs[image_index], VK_TRUE,
		std::numeric_limits<uint64_t>::max()) != VK_SUCCESS)
		throw std::runtime_error("failed to wait for fence!");
	t.stop();
	std::cout << "swap chain pass wait: " << t.get() << "\n";

	//record secondary command buffers (if necessary)

	constexpr std::array<uint32_t, 2> deferred_rend_types = {
		LIFE_EXPECTANCY_COUNT*RENDERABLE_TYPE_MAT_OPAQUE,
		LIFE_EXPECTANCY_COUNT*RENDERABLE_TYPE_MAT_OPAQUE + 1
	};
	for (auto i : deferred_rend_types)
	{
		if (m_record_masks[image_index][i] && !m_renderables[i].empty())
		{

			auto cb = m_secondary_cbs[i][image_index];
			uint32_t rend_type = i / LIFE_EXPECTANCY_COUNT;

			VkCommandBufferInheritanceInfo inheritance = {};
			inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inheritance.framebuffer = m_fbs[image_index];
			inheritance.occlusionQueryEnable = VK_FALSE;
			inheritance.renderPass = m_pass;
			inheritance.subpass = 0;

			VkCommandBufferBeginInfo cb_begin = {};
			cb_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cb_begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
			cb_begin.pInheritanceInfo = &inheritance;

			vkBeginCommandBuffer(cb, &cb_begin);
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[rend_type]);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pls[rend_type], 0, 1,
				&m_per_frame_dss[rend_type], 0, nullptr);

			for (const auto& r : m_renderables[i])
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pls[rend_type], 1, 1, &r.tr_ds, 0, nullptr);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pls[rend_type], 2, 1, &r.mat_light_ds, 0, nullptr);
				vkCmdBindIndexBuffer(cb, r.m.ib, 0, VK_INDEX_TYPE_UINT32);
				VkBuffer vbs[] = { r.m.vb, r.m.veb };
				if (vbs[1] == VK_NULL_HANDLE)
					vbs[1] = vbs[0];
				VkDeviceSize offsets[] = { 0,0 };
				vkCmdBindVertexBuffers(cb, 0, 2, vbs, offsets);
				vkCmdDrawIndexed(cb, r.m.size, 1, 0, 0, 0);
			}

			if (vkEndCommandBuffer(cb) != VK_SUCCESS)
				throw std::runtime_error("failed to record material command buffer!");
		}	
	}

	constexpr std::array<uint32_t, 2> light_pass_rend_types = {
		LIFE_EXPECTANCY_COUNT*RENDERABLE_TYPE_LIGHT_OMNI,
		LIFE_EXPECTANCY_COUNT*RENDERABLE_TYPE_LIGHT_OMNI + 1
	};

	for (auto i : light_pass_rend_types)
	{
		if (m_record_masks[image_index][i] && !m_renderables[i].empty())
		{
			auto cb = m_secondary_cbs[i][image_index];
			int rend_type = i / LIFE_EXPECTANCY_COUNT;

			VkCommandBufferInheritanceInfo inheritance = {};
			inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inheritance.framebuffer = m_fbs[image_index];
			inheritance.occlusionQueryEnable = VK_FALSE;
			inheritance.renderPass = m_pass;
			inheritance.subpass = 1;

			VkCommandBufferBeginInfo cb_begin = {};
			cb_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cb_begin.flags =  VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
			cb_begin.pInheritanceInfo = &inheritance;

			vkBeginCommandBuffer(cb, &cb_begin);
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[rend_type]);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pls[rend_type], 1, 1, &m_gbuffer_ds, 0, nullptr);
			for (auto& l : m_renderables[i])
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pls[rend_type], 0, 1, &l.mat_light_ds, 0, nullptr);
				vkCmdDraw(cb, 4, 1, 0, 0);
			}
			if (vkEndCommandBuffer(cb) != VK_SUCCESS)
				throw std::runtime_error("failed to record light command buffer!");
		}
	}

	constexpr std::array<uint32_t, 2> skybox_pass_rend_types = {
		LIFE_EXPECTANCY_COUNT*RENDERABLE_TYPE_SKYBOX,
		LIFE_EXPECTANCY_COUNT*RENDERABLE_TYPE_SKYBOX + 1
	};
	for (auto i : skybox_pass_rend_types)
	{
		if (m_record_masks[image_index][i] && !m_renderables[i].empty())
		{
			auto cb = m_secondary_cbs[i][image_index];
			int rend_type = i / LIFE_EXPECTANCY_COUNT;

			VkCommandBufferInheritanceInfo inheritance = {};
			inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inheritance.framebuffer = m_fbs[image_index];
			inheritance.occlusionQueryEnable = VK_FALSE;
			inheritance.renderPass = m_pass;
			inheritance.subpass = 2;

			VkCommandBufferBeginInfo cb_begin = {};
			cb_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cb_begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
			cb_begin.pInheritanceInfo = &inheritance;

			vkBeginCommandBuffer(cb, &cb_begin);
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[rend_type]);
			VkDescriptorSet sets[2] = { m_per_frame_dss[RENDERABLE_TYPE_MAT_OPAQUE], m_renderables[i][0].mat_light_ds};
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pls[RENDERABLE_TYPE_SKYBOX],
				0, 2, sets, 0, nullptr);
			vkCmdDraw(cb, 1, 1, 0, 0);

			if (vkEndCommandBuffer(cb) != VK_SUCCESS)
				throw std::runtime_error("failed to record light command buffer!");
		
		}
	}

	m_record_masks[image_index].reset();


	//record primary cb

	auto cb = m_primary_cbs[image_index];
	VkCommandBufferBeginInfo cb_begin = {};
	cb_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cb_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(cb, &cb_begin);

	VkRenderPassBeginInfo pass_begin = {};
	pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	pass_begin.framebuffer = m_fbs[image_index];

	std::array<VkClearValue, 7> clear_values = {};
	clear_values[7].color = { 0.f, 0.f, 0.f, 1.f };
	clear_values[0].depthStencil = { 1.0f, 0 };
	pass_begin.clearValueCount = 7;
	pass_begin.pClearValues = clear_values.data();

	pass_begin.renderArea.offset = { 0,0 };
	pass_begin.renderArea.extent = m_base.swap_chain_image_extent;
	pass_begin.renderPass = m_pass;

	vkCmdBeginRenderPass(cb, &pass_begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	for (auto j : deferred_rend_types)
	{
		if (!m_renderables[j].empty())
			vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[j][image_index]);
	}
	vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	for (auto j : light_pass_rend_types)
	{
		if (!m_renderables[j].empty())
			vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[j][image_index]);
	}

	vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	for (auto j : skybox_pass_rend_types)
	{
		if (!m_renderables[j].empty())
			vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[j][image_index]);
	}

	vkCmdEndRenderPass(cb);
		
	if (vkEndCommandBuffer(cb) != VK_SUCCESS)
		throw std::runtime_error("failed to record command buffer!");


	//submit primary cb
	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cb;
	VkSemaphore signal_semaphores[2] = { m_render_finished_s, m_render_finished_s2 };
	submit.pSignalSemaphores = signal_semaphores;
	submit.signalSemaphoreCount = 2;
	VkSemaphore wait_semaphores[2] = { wait_s, m_image_available_s };
	submit.pWaitSemaphores = wait_semaphores;
	submit.waitSemaphoreCount = 2;
	VkPipelineStageFlags wait_stage[2] = { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT ,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit.pWaitDstStageMask = wait_stage;

	vkResetFences(m_base.device, 1, &m_primary_cb_finished_fs[image_index]);
	if (vkQueueSubmit(m_base.graphics_queue, 1, &submit, m_primary_cb_finished_fs[image_index]) != VK_SUCCESS)
		throw std::runtime_error("failed to submit command buffer!");

	VkPresentInfoKHR present = {};
	present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.pImageIndices = &image_index;
	present.pSwapchains = &m_base.swap_chain;
	present.pWaitSemaphores = &m_render_finished_s;
	present.swapchainCount = 1;
	present.waitSemaphoreCount = 1;

	if (vkQueuePresentKHR(m_base.present_queue, &present) != VK_SUCCESS)
		throw std::runtime_error("failed to present image!");
	
	return m_render_finished_s2;
}

void basic_pass::allocate_command_buffers()
{
	//allocate secondary cbs
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandBufferCount = static_cast<uint32_t>(m_fbs.size());
	alloc_info.commandPool = m_cp;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	for (auto& cbs : m_secondary_cbs)
	{
		cbs.resize(m_fbs.size());
		if (vkAllocateCommandBuffers(m_base.device, &alloc_info, cbs.data()) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate secondary commant buffers!");	
	}

	//allocate primary cbs
	m_primary_cbs.resize(m_fbs.size());
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	if (vkAllocateCommandBuffers(m_base.device, &alloc_info, m_primary_cbs.data()) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate primary command buffers!");

}

void basic_pass::create_descriptors()
{
	//create per frame descriptor set layouts
	VkDescriptorSetLayoutBinding cam_binding = {};
	cam_binding.binding = 0;
	cam_binding.descriptorCount = 1;
	cam_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cam_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;

	VkDescriptorSetLayoutCreateInfo dsl_info = {};
	dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsl_info.bindingCount = 1;
	dsl_info.pBindings = &cam_binding;
	if (vkCreateDescriptorSetLayout(m_base.device, &dsl_info, host_memory_manager, &m_per_frame_dsls[RENDERABLE_TYPE_MAT_OPAQUE]) 
		!= VK_SUCCESS)
		throw std::runtime_error("failed to create per frame descriptor set layout!");

	//create gbuffer descriptos set layout
	VkDescriptorSetLayoutBinding gbuffer_bindings[5];
	VkDescriptorSetLayoutBinding gbuffer_binding = {};
	gbuffer_binding.descriptorCount = 1;
	gbuffer_binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	gbuffer_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	
	for (uint32_t i = 0; i < 5; ++i)
	{
		gbuffer_binding.binding = i;
		gbuffer_bindings[i] = gbuffer_binding;
	}

	VkDescriptorSetLayoutCreateInfo gbuffer_dsl_info = {};
	gbuffer_dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	gbuffer_dsl_info.bindingCount = 5;
	gbuffer_dsl_info.pBindings = gbuffer_bindings;
	if (vkCreateDescriptorSetLayout(m_base.device, &gbuffer_dsl_info, host_memory_manager, &m_gbuffer_dsl) != VK_SUCCESS)
		throw std::runtime_error("failed to create gbuffer descriptor set layout!");

	//create descriptor pool
	VkDescriptorPoolSize pool_size[2] = {};
	pool_size[0].descriptorCount = 1;
	pool_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	pool_size[1].descriptorCount = 5;
	pool_size[1].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.maxSets = 2;
	pool_info.poolSizeCount = 2;
	pool_info.pPoolSizes = pool_size;
	if (vkCreateDescriptorPool(m_base.device, &pool_info, host_memory_manager, &m_dp) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor pool!");

	//create per frame descriptor sets
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = m_dp;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &m_per_frame_dsls[RENDERABLE_TYPE_MAT_OPAQUE];
	if (vkAllocateDescriptorSets(m_base.device, &alloc_info, &m_per_frame_dss[RENDERABLE_TYPE_MAT_OPAQUE]) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate per frame descriptor set!");

	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = m_per_frame_b;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(camera_data);

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.dstArrayElement = 0;
	write.dstBinding = 0;
	write.dstSet = m_per_frame_dss[RENDERABLE_TYPE_MAT_OPAQUE];
	write.pBufferInfo = &buffer_info;
	
	vkUpdateDescriptorSets(m_base.device, 1, &write, 0, nullptr);

	//create gbuffer descriptor sets
	alloc_info.descriptorPool = m_dp;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &m_gbuffer_dsl;
	if (vkAllocateDescriptorSets(m_base.device, &alloc_info, &m_gbuffer_ds) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate per frame descriptor set!");

	std::array<VkDescriptorImageInfo, 5> gbuffer_image_infos = {};
	std::array<VkWriteDescriptorSet, 5> writes = {};
	for (uint32_t i=0; i<gbuffer_image_infos.size(); ++i)
	{
		gbuffer_image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbuffer_image_infos[i].imageView = m_gbuffer_views[i];

		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		writes[i].dstArrayElement = 0;
		writes[i].dstBinding = i;
		writes[i].dstSet = m_gbuffer_ds;
		writes[i].pImageInfo = &gbuffer_image_infos[i];
	}

	vkUpdateDescriptorSets(m_base.device, 5, writes.data(), 0, nullptr);
}

void basic_pass::create_semaphores()
{
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	
	if (vkCreateSemaphore(m_base.device, &info, host_memory_manager, &m_image_available_s) != VK_SUCCESS ||
		vkCreateSemaphore(m_base.device, &info, host_memory_manager, &m_render_finished_s) != VK_SUCCESS ||
		vkCreateSemaphore(m_base.device, &info, host_memory_manager, &m_render_finished_s2) != VK_SUCCESS)
		throw std::runtime_error("failed to create semaphores!");
}

void basic_pass::create_fences()
{
	VkFenceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	m_primary_cb_finished_fs.resize(m_primary_cbs.size());
	for (auto& f : m_primary_cb_finished_fs)
	{
		if (vkCreateFence(m_base.device, &info, host_memory_manager, &f) != VK_SUCCESS)
			throw std::runtime_error("failed to create fence!");
	}
}

void basic_pass::wait_for_finish()
{
	if (vkWaitForFences(m_base.device, static_cast<uint32_t>(m_primary_cb_finished_fs.size()), m_primary_cb_finished_fs.data(), VK_TRUE,
		std::numeric_limits<uint64_t>::max()) != VK_SUCCESS)
		throw std::runtime_error("failed to wait for fences!");
}

void basic_pass::update_frustum_points()
{
	for (float i = 0; i < FRUSTUM_SPLIT_COUNT+1; ++i)
	{
		for (uint32_t j = 0; j < 4; ++j)
		{
			glm::vec3 p;
			p.z = (i / FRUSTUM_SPLIT_COUNT)*(i / FRUSTUM_SPLIT_COUNT)*(m_proj.far - m_proj.near) + m_proj.near;
			p.x = m_proj.proj[0][0] * p.z*((j|1)*2-1);
			p.y = m_proj.proj[1][1] * p.z*((j|2)-1);
			m_frustum_points[i][j] = p;
		}
	}
}

void basic_pass::create_dir_shadow_map_pipeline()
{
	auto vert_code = read_file("shaders/special_shaders/dir_shadow_map/dirs_shadow_map.vert");
	auto geom_code = read_file("shaders/special_shaders/dir_shadow_map/dir_shadow_map.geom");

	VkShaderModule vert_module = create_shader_module(m_base.device, vert_code);
	VkShaderModule geom_module = create_shader_module(m_base.device, geom_code);

	VkPipelineShaderStageCreateInfo shaders[2] = {};
	shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[0].module = vert_module;
	shaders[0].pName = "main";
	shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[1].module = geom_module;
	shaders[1].pName = "main";
	shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;

	VkPipelineVertexInputStateCreateInfo input = {};
	input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	constexpr auto binding_description = vertex::get_binding_description();
	constexpr auto attribute_description = vertex::get_attribute_descriptions()[0];
	input.pVertexAttributeDescriptions = &attribute_description;
	input.pVertexBindingDescriptions = &binding_description;
	input.vertexAttributeDescriptionCount = 1;
	input.vertexBindingDescriptionCount = 1;

	VkPipelineInputAssemblyStateCreateInfo assembly = {};
	assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.primitiveRestartEnable = VK_FALSE;
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkViewport vp = {};
	vp.height = (float)DIR_SHADOW_MAP_SIZE;
	vp.width = (float)DIR_SHADOW_MAP_SIZE;
	vp.minDepth = 0.f;
	vp.maxDepth = 1.f;

	VkRect2D scissor = {};
	scissor.extent = { DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE };
	scissor.offset = { 0,0 };

	VkPipelineViewportStateCreateInfo viewport = {};
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.pScissors = &scissor;
	viewport.pViewports = &vp;
	viewport.scissorCount = 1;
	viewport.viewportCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.depthBiasClamp = 0.f;
	rasterizer.depthBiasConstantFactor = 0.f;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasSlopeFactor = 0.f;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.lineWidth = 1.f;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisample = {};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.alphaToCoverageEnable = VK_FALSE;
	multisample.alphaToOneEnable = VK_FALSE;
	multisample.sampleShadingEnable = VK_FALSE;
	multisample.minSampleShading = 0.f;
	multisample.sampleShadingEnable = VK_FALSE;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depth = {};
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth.depthBoundsTestEnable = VK_FALSE;
	depth.depthCompareOp = VK_COMPARE_OP_LESS;
	depth.depthTestEnable = VK_TRUE;
	depth.depthWriteEnable = VK_TRUE;
	depth.stencilTestEnable = VK_FALSE;
	depth.maxDepthBounds = 1.f;
	depth.minDepthBounds = 0.f;

	VkPipelineLayoutCreateInfo layout = {};
	layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	VkDescriptorSetLayout dss[2] = { resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_DIR),
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_TR) };
	layout.setLayoutCount = 2;
	layout.pSetLayouts = dss;

	if (vkCreatePipelineLayout(m_base.device, &layout, host_memory_manager, &m_pl_dir_shadow) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!"); 

	VkGraphicsPipelineCreateInfo gp_info = {};
	gp_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gp_info.basePipelineHandle = VK_NULL_HANDLE;
	gp_info.basePipelineIndex = -1;
	gp_info.layout = m_pl_dir_shadow;
	gp_info.pDepthStencilState = &depth;
	gp_info.pInputAssemblyState = &assembly;
	gp_info.pMultisampleState = &multisample;
	gp_info.pRasterizationState = &rasterizer;
	gp_info.pStages = shaders;
	gp_info.pVertexInputState = &input;
	gp_info.pViewportState = &viewport;
	gp_info.renderPass = m_pass;
	gp_info.stageCount = 2;
	gp_info.subpass = 0;

	if (vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, 1, &gp_info, host_memory_manager, &m_gp_dir_shadow) != VK_SUCCESS)
		throw std::runtime_error("failed to create dir light shadow pipeline!");

	vkDestroyShaderModule(m_base.device, vert_module, host_memory_manager);
	vkDestroyShaderModule(m_base.device, geom_module, host_memory_manager);
}

