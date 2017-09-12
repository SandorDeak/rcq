#include "basic_pass.h"

using namespace rcq;

basic_pass* basic_pass::m_instance = nullptr;

basic_pass::basic_pass(const base_info& info): m_base(info)
{
	create_render_pass();
	create_descriptor_set_layouts();
}


basic_pass::~basic_pass()
{
	vkDestroyPipelineLayout(m_base.device, m_pl, host_memory_manager);
	vkDestroyPipeline(m_base.device, m_gp, host_memory_manager);
	vkDestroyDescriptorSetLayout(m_base.device, m_cam_dsl, host_memory_manager);
	vkDestroyDescriptorSetLayout(m_base.device, m_tr_dsl, host_memory_manager);
	vkDestroyRenderPass(m_base.device, m_pass, host_memory_manager);
}

void basic_pass::init(const base_info& info)
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("core is already initialised!");
	}
	m_instance = new basic_pass(info);
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
	VkAttachmentDescription color_attachment = {};
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	color_attachment.format = m_base.swap_chain_image_format;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
	
	VkAttachmentDescription depth_attachment = {};
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.format = find_depth_format(m_base.physical_device);
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

	VkAttachmentReference color_ref = {};
	color_ref.attachment = 0;
	color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_ref = {};
	depth_ref.attachment = 1;
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;
	subpass.pDepthStencilAttachment = &depth_ref;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[2] = { color_attachment, depth_attachment };

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 2;
	info.dependencyCount = 1;
	info.pAttachments = attachments;
	info.pDependencies = &dependency;
	info.pSubpasses = &subpass;
	info.subpassCount = 1;

	if (vkCreateRenderPass(m_base.device, &info, host_memory_manager, &m_pass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

void basic_pass::create_descriptor_set_layouts()
{
	//camera dsl
	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = 0;
	binding.descriptorCount = 1;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	
	VkDescriptorSetLayoutCreateInfo dsl_info = {};
	dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsl_info.bindingCount = 1;
	dsl_info.pBindings = &binding;

	if (vkCreateDescriptorSetLayout(m_base.device, &dsl_info, host_memory_manager, &m_cam_dsl) != VK_SUCCESS)
		throw std::runtime_error("failed to create camera dsl!");

	//transform dsl
	if (vkCreateDescriptorSetLayout(m_base.device, &dsl_info, host_memory_manager, &m_tr_dsl) != VK_SUCCESS)
		throw std::runtime_error("failed to create transform dsl!");
}

void basic_pass::create_graphics_pipeline()
{
	auto vert_code = read_file("shaders/basic_pass/vert.spv");
	auto frag_code = read_file("shaders/basic_pass/frag.spv");

	auto vert_module = create_shader_module(m_base.device, vert_code);
	auto frag_module = create_shader_module(m_base.device, frag_code);

	VkPipelineShaderStageCreateInfo shaders[2];
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
	constexpr auto attribs = vertex::get_attribute_descriptions();
	constexpr auto binding = vertex::get_binding_description();
	vertex_input.pVertexAttributeDescriptions = attribs.data();
	vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribs.size());
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexBindingDescriptionCount = 1;
	
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

	VkPipelineColorBlendStateCreateInfo color_blend = {};
	color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend.attachmentCount = 1;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.pAttachments=&color_blend_attachment;

	VkPipelineLayoutCreateInfo layout = {};
	layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	VkDescriptorSetLayout dsls[] = { m_cam_dsl, m_tr_dsl };
	layout.pSetLayouts = dsls;
	layout.setLayoutCount = 2;
	
	if (vkCreatePipelineLayout(m_base.device, &layout, host_memory_manager, &m_pl) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!");

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.basePipelineHandle = VK_NULL_HANDLE;
	info.basePipelineIndex = -1;
	info.layout = m_pl;
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

	if (vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, 1, &info, host_memory_manager, &m_gp) != VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline!");

	vkDestroyShaderModule(m_base.device, vert_module, host_memory_manager);
	vkDestroyShaderModule(m_base.device, frag_module, host_memory_manager);
}


