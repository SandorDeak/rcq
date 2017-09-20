#include "basic_pass.h"

#include "resource_manager.h"

using namespace rcq;

basic_pass* basic_pass::m_instance = nullptr;

basic_pass::basic_pass(const base_info& info, const renderable_container& renderables): m_base(info), m_renderables(renderables)
{
	create_render_pass();
	create_descriptor_set_layout();
	create_graphics_pipelines();
	create_command_pool();
	create_depth_and_per_frame_buffer();
	create_per_frame_resources();
	create_framebuffers();
	allocate_command_buffers();
}


basic_pass::~basic_pass()
{
	for (auto& dsl : m_per_frame_dsls)
		vkDestroyDescriptorSetLayout(m_base.device, dsl, host_memory_manager);
	vkDestroyDescriptorPool(m_base.device, m_per_frame_dp, host_memory_manager);
	
	vkUnmapMemory(m_base.device, m_per_frame_b_mem);
	vkDestroyBuffer(m_base.device, m_per_frame_b, host_memory_manager);

	for (auto& fb : m_fbs)
		vkDestroyFramebuffer(m_base.device, fb, host_memory_manager);

	vkDestroyCommandPool(m_base.device, m_cp, host_memory_manager);

	vkDestroyImageView(m_base.device, m_depth_tex.view, host_memory_manager);
	vkDestroyImage(m_base.device, m_depth_tex.image, host_memory_manager);
	auto destroy=std::make_unique<destroy_package>();
	destroy->ids[RESOURCE_TYPE_MEMORY].push_back(RENDER_PASS_BASIC);
	resource_manager::instance()->push_destroy_package(std::move(destroy));


	for(auto& pl : m_pls)
		vkDestroyPipelineLayout(m_base.device, pl, host_memory_manager);
	for(auto& gp : m_gps)
		vkDestroyPipeline(m_base.device, gp, host_memory_manager);
	vkDestroyDescriptorSetLayout(m_base.device, m_cam_dsl, host_memory_manager);
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

void basic_pass::create_graphics_pipelines()
{
	auto vert_code = read_file("shaders/basic_pass/vert.spv");
	auto frag_code = read_file("shaders/basic_pass/frag.spv");

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
	VkDescriptorSetLayout dsls[] = { m_cam_dsl, resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_TR), 
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_MAT) };
	layout.pSetLayouts = dsls;
	layout.setLayoutCount = 3;
	
	if (vkCreatePipelineLayout(m_base.device, &layout, host_memory_manager, &m_pls[MAT_TYPE_BASIC]) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!");

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.basePipelineHandle = VK_NULL_HANDLE;
	info.basePipelineIndex = -1;
	info.layout = m_pls[MAT_TYPE_BASIC];
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

	if (vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, 1, &info, host_memory_manager, &m_gps[MAT_TYPE_BASIC]) != VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline!");

	vkDestroyShaderModule(m_base.device, vert_module, host_memory_manager);
	vkDestroyShaderModule(m_base.device, frag_module, host_memory_manager);
}

void basic_pass::create_descriptor_set_layout()
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
}

void basic_pass::create_depth_and_per_frame_buffer()
{
	VkFormat depth_format = find_depth_format(m_base.physical_device);
	//create image
	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.arrayLayers = 1;
	image_info.extent.height = m_base.swap_chain_image_extent.height;
	image_info.extent.width = m_base.swap_chain_image_extent.width;
	image_info.extent.depth = 1;
	image_info.format = depth_format;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.mipLevels = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	if (vkCreateImage(m_base.device, &image_info, host_memory_manager,
		&m_depth_tex.image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create depth image!");
	}

	//create per frame buffer
	VkBufferCreateInfo buffer_create_info = {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_create_info.size = sizeof(camera_data);
	buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (vkCreateBuffer(m_base.device, &buffer_create_info, host_memory_manager, &m_per_frame_b) != VK_SUCCESS)
		throw std::runtime_error("failed to create per frame buffer!");

	//send memory build package to resource manager
	VkMemoryRequirements image_mr, buffer_mr;
	vkGetImageMemoryRequirements(m_base.device, m_depth_tex.image, &image_mr);
	vkGetBufferMemoryRequirements(m_base.device, m_per_frame_b, &buffer_mr);

	std::vector<VkMemoryAllocateInfo> alloc_info(2);
	alloc_info[0].sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info[0].allocationSize = image_mr.size;
	alloc_info[0].memoryTypeIndex = find_memory_type(m_base.physical_device, image_mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	alloc_info[1].sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info[1].allocationSize = buffer_mr.size;
	alloc_info[1].memoryTypeIndex = find_memory_type(m_base.physical_device, buffer_mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	build_package build;
	std::get<RESOURCE_TYPE_MEMORY>(build).emplace_back(RENDER_PASS_BASIC, std::move(alloc_info));
	resource_manager::instance()->process_build_package(std::move(build));

	//bind memory to image and buffer and map
	memory mem = resource_manager::instance()->get<RESOURCE_TYPE_MEMORY>(RENDER_PASS_BASIC);
	vkBindImageMemory(m_base.device, m_depth_tex.image, mem[0], 0);
	vkBindBufferMemory(m_base.device, m_per_frame_b, mem[1], 0);
	m_per_frame_b_mem = mem[1];
	void* raw_data;
	vkMapMemory(m_base.device, m_per_frame_b_mem, 0, sizeof(camera_data), 0, &raw_data);
	m_per_frame_b_data = static_cast<char*>(raw_data);

	//create view
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


	//transition to depth stencial attachment optimal layout
	VkCommandBuffer cb = begin_single_time_command(m_base.device, m_cp);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_depth_tex.image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (depth_format == VK_FORMAT_D24_UNORM_S8_UINT || depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT)
		barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	vkCmdPipelineBarrier(cb,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
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
	std::array<VkImageView, 2> attachments = {};
	attachments[1] = m_depth_tex.view;

	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.attachmentCount = 2;
	fb_info.pAttachments = attachments.data();
	fb_info.layers = 1;
	fb_info.height = m_base.swap_chain_image_extent.height;
	fb_info.width = m_base.swap_chain_image_extent.width;
	fb_info.renderPass = m_pass;

	m_fbs.resize(m_base.swap_chain_image_views.size());
	for (size_t i = 0; i < m_fbs.size(); ++i)
	{
		attachments[0] = m_base.swap_chain_image_views[i];
		if (vkCreateFramebuffer(m_base.device, &fb_info, host_memory_manager, &m_fbs[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create framebuffer!");	
	}
}

void basic_pass::record_and_render(const std::optional<camera_data>& cam, std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> record_maks)
{
	//record secondary command buffers (if necessary)
	for (uint32_t i = 0; i < MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT; ++i)
	{
		if (record_maks[i] && !m_renderables[i].empty())
		{
			for (uint32_t j = 0; j < m_secondary_cbs[i].size(); ++j)
			{
				auto cb = m_secondary_cbs[i][j];
				uint32_t mat_type = i / LIFE_EXPECTANCY_COUNT;

				VkCommandBufferInheritanceInfo inheritance = {};
				inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
				inheritance.framebuffer = m_fbs[j];
				inheritance.occlusionQueryEnable = VK_FALSE;
				inheritance.renderPass = m_pass;
				inheritance.subpass = 0;

				VkCommandBufferBeginInfo cb_begin = {};
				cb_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				cb_begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
				cb_begin.pInheritanceInfo = &inheritance;

				vkBeginCommandBuffer(cb, &cb_begin);
				vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[mat_type]);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_per_frame_dsls[mat_type], 0, 1,
					&m_per_frame_dss[mat_type], 0, nullptr);

				for (auto& r : m_renderables[i])
				{
					vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pls[mat_type], 1, 2, &r.tr_ds, 0, nullptr);
					vkCmdBindIndexBuffer(cb, r.ib, 0, VK_INDEX_TYPE_UINT32);
					VkBuffer vbs[] = { r.vb, r.veb };
					if (vbs[1] == VK_NULL_HANDLE)
						vbs[1] = vbs[0];
					VkDeviceSize offsets[] = { 0,0 };
					vkCmdBindVertexBuffers(cb, 0, 2, vbs, offsets);
					vkCmdDrawIndexed(cb, r.size, 1, 0, 0, 0);
				}

				if (vkEndCommandBuffer(cb) != VK_SUCCESS)
					throw std::runtime_error("failed to record command buffer!");
			}

		}
	}

	//acquire swap chain image
	uint32_t image_index;

	if (vkAcquireNextImageKHR(m_base.device, m_base.swap_chain, std::numeric_limits<uint64_t>::max(), m_image_available_s,
		VK_NULL_HANDLE, &image_index) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}
	// copy camera data
	if (cam)
		memcpy(m_per_frame_b_data, &cam.value(), sizeof(camera_data));

	//record primary cb

	auto cb = m_primary_cbs[image_index];
	VkCommandBufferBeginInfo cb_begin = {};
	cb_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cb_begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	vkBeginCommandBuffer(cb, &cb_begin);

	VkRenderPassBeginInfo pass_begin = {};
	pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	pass_begin.framebuffer = m_fbs[image_index];

	std::array<VkClearValue, 2> clear_values = {};
	clear_values[0].color = { 0.f, 0.f, 0.f, 1.f };
	clear_values[1].depthStencil = { 1.0f, 0 };
	pass_begin.clearValueCount = 2;
	pass_begin.pClearValues = clear_values.data();

	pass_begin.renderArea.offset = { 0,0 };
	pass_begin.renderArea.extent = m_base.swap_chain_image_extent;
	pass_begin.renderPass = m_pass;

	vkCmdBeginRenderPass(cb, &pass_begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	for (uint32_t j = 0; j < MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT; ++j)
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
	submit.pSignalSemaphores = &m_render_finished_s;
	submit.signalSemaphoreCount = 1;
	submit.pWaitSemaphores = &m_image_available_s;
	submit.waitSemaphoreCount = 1;
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submit.pWaitDstStageMask = &wait_stage;

	if (vkQueueSubmit(m_base.graphics_queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
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
}

void basic_pass::allocate_command_buffers()
{
	//allocate econdary cbs
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

void basic_pass::create_per_frame_resources()
{
	//create per frame descriptor set layouts
	VkDescriptorSetLayoutBinding cam_binding = {};
	cam_binding.binding = 0;
	cam_binding.descriptorCount = 1;
	cam_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cam_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo dsl_info = {};
	dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsl_info.bindingCount = 1;
	dsl_info.pBindings = &cam_binding;
	if (vkCreateDescriptorSetLayout(m_base.device, &dsl_info, host_memory_manager, &m_per_frame_dsls[MAT_TYPE_BASIC]) != VK_SUCCESS)
		throw std::runtime_error("failed to create per frame descriptor set layout!");

	//create per frame descriptor pool
	VkDescriptorPoolSize pool_size = {};
	pool_size.descriptorCount = 1;
	pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.maxSets = 1;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;
	if (vkCreateDescriptorPool(m_base.device, &pool_info, host_memory_manager, &m_per_frame_dp) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor pool!");

	//create per frame descriptor sets
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = m_per_frame_dp;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &m_per_frame_dsls[MAT_TYPE_BASIC];
	if (vkAllocateDescriptorSets(m_base.device, &alloc_info, &m_per_frame_dss[MAT_TYPE_BASIC]) != VK_SUCCESS)
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
	write.dstSet = m_per_frame_dss[MAT_TYPE_BASIC];
	write.pBufferInfo = &buffer_info;
	
	vkUpdateDescriptorSets(m_base.device, 1, &write, 0, nullptr);
}