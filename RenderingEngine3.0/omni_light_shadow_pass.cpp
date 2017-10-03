#include "omni_light_shadow_pass.h"
#include "resource_manager.h"

using namespace rcq;

omni_light_shadow_pass* omni_light_shadow_pass::m_instance = nullptr;

omni_light_shadow_pass::omni_light_shadow_pass(const base_info& info, const renderable_container& renderables,
	const light_renderable_container& light_renderables):
	m_base(info),
	m_renderables(renderables),
	m_light_renderables(light_renderables)
{
	create_render_pass();
	create_command_pool();
	create_pipeline();
	allocate_command_buffer();
	create_semaphore();
	create_fence();
}


omni_light_shadow_pass::~omni_light_shadow_pass()
{
	vkDestroyFence(m_base.device, m_render_finished_f, host_memory_manager);
	vkDestroySemaphore(m_base.device, m_render_finished_s, host_memory_manager);
	vkDestroyPipelineLayout(m_base.device, m_pl, host_memory_manager);
	vkDestroyPipeline(m_base.device, m_gp, host_memory_manager);
	vkDestroyCommandPool(m_base.device, m_cp, host_memory_manager);
	vkDestroyRenderPass(m_base.device, m_pass, host_memory_manager);
}

void omni_light_shadow_pass::init(const base_info& info, const renderable_container& renderables,
	const light_renderable_container& light_renderables)
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("omni light shadow pass is already initialised!");
	}
	m_instance = new omni_light_shadow_pass(info, renderables, light_renderables);
}

void omni_light_shadow_pass::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy omni light shadow pass, it doesn't exist!");
	}

	delete m_instance;
}

void omni_light_shadow_pass::create_render_pass()
{
	//struct light should have frame buffer, or framebuffer should be created on the fly!!!

	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format = VK_FORMAT_D32_SFLOAT;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

	VkAttachmentReference depth_ref = {};
	depth_ref.attachment = 0;
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pDepthStencilAttachment = &depth_ref;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	VkRenderPassCreateInfo pass_info = {};
	pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	pass_info.attachmentCount = 1;
	pass_info.pAttachments = &depth_attachment;
	pass_info.pSubpasses = &subpass;
	pass_info.subpassCount = 1;
	
	if (vkCreateRenderPass(m_base.device, &pass_info, host_memory_manager, &m_pass) != VK_SUCCESS)
		throw std::runtime_error("failed to create omni light shadow pass!");
}

void omni_light_shadow_pass::create_command_pool()
{
	VkCommandPoolCreateInfo cp_info = {};
	cp_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cp_info.queueFamilyIndex = m_base.queue_families.graphics_family;
	cp_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(m_base.device, &cp_info, host_memory_manager, &m_cp) != VK_SUCCESS)
		throw std::runtime_error("failed to create command pool!");
}

void omni_light_shadow_pass::create_pipeline()
{
	auto vert_code = read_file("shaders/shadow_map_pass/omni_light_shadow_map/vert.spv");
	auto geom_code= read_file("shaders/shadow_map_pass/omni_light_shadow_map/geom.spv");

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
	vp.height = (float)SHADOW_MAP_SIZE;
	vp.width = (float)SHADOW_MAP_SIZE;
	vp.minDepth = 0.f;
	vp.maxDepth = 1.f;

	VkRect2D scissor = {};
	scissor.extent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
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
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
	VkDescriptorSetLayout dss[2] = { resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT),
		resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_TR) };
	layout.setLayoutCount = 2;
	layout.pSetLayouts = dss;
	
	if (vkCreatePipelineLayout(m_base.device, &layout, host_memory_manager, &m_pl) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!");

	VkGraphicsPipelineCreateInfo gp_info = {};
	gp_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gp_info.basePipelineHandle = VK_NULL_HANDLE;
	gp_info.basePipelineIndex = -1;
	gp_info.layout = m_pl;
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

	if (vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, 1, &gp_info, host_memory_manager, &m_gp) != VK_SUCCESS)
		throw std::runtime_error("failed to create omni light shadow pass pipeline!");

	vkDestroyShaderModule(m_base.device, vert_module, host_memory_manager);
	vkDestroyShaderModule(m_base.device, geom_module, host_memory_manager);
}

void omni_light_shadow_pass::allocate_command_buffer()
{
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandBufferCount = 1;
	alloc_info.commandPool = m_cp;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	if (vkAllocateCommandBuffers(m_base.device, &alloc_info, &m_cb) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate command buffer!");
}

void omni_light_shadow_pass::record_and_render(const std::optional<camera_data>& cam, std::bitset<MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> mat_record_mass, std::bitset<LIGHT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> light_record_mask)
{
	//record cb
	VkCommandBufferBeginInfo cb_begin = {};
	cb_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cb_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(m_cb, &cb_begin);

	for (auto& l : m_light_renderables[LIGHT_TYPE_OMNI * 2])
	{
		if (l.shadow_map.image != VK_NULL_HANDLE)
		{
			VkImageMemoryBarrier barrier0 = {};
			barrier0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier0.image = l.shadow_map.image;
			barrier0.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier0.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			barrier0.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier0.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barrier0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier0.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			barrier0.subresourceRange.baseArrayLayer = 0;
			barrier0.subresourceRange.baseMipLevel = 0;
			barrier0.subresourceRange.layerCount = 6;
			barrier0.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(m_cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier0);

			VkRenderPassBeginInfo pass_begin = {};
			pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			pass_begin.clearValueCount = 1;
			VkClearValue clear;
			clear.depthStencil = { 1.f, 0 };
			pass_begin.pClearValues = &clear;
			pass_begin.framebuffer = l.shadow_map_fb;
			pass_begin.renderPass = m_pass;
			pass_begin.renderArea.extent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
			pass_begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(m_cb, &pass_begin, VK_SUBPASS_CONTENTS_INLINE);


			vkCmdBindPipeline(m_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gp);
			vkCmdBindDescriptorSets(m_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pl, 0, 1, &l.ds, 0, nullptr);
			std::array<int, 2> opaque_mat_types = { 2 * MAT_TYPE_BASIC, 2 * MAT_TYPE_BASIC + 1 };
			for (int mat_type : opaque_mat_types)
			{
				for (auto& r : m_renderables[mat_type])
				{
					vkCmdBindDescriptorSets(m_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pl, 1, 1, &r.tr_ds, 0, nullptr);
					VkDeviceSize offset = 0;
					vkCmdBindVertexBuffers(m_cb, 0, 1, &r.vb, &offset);
					vkCmdBindIndexBuffer(m_cb, r.ib, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(m_cb, r.size, 1, 0, 0, 0);
				}
			}
			vkCmdEndRenderPass(m_cb);

			VkImageMemoryBarrier barrier1 = {};
			barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier1.image = l.shadow_map.image;
			barrier1.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			barrier1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier1.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barrier1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			barrier1.subresourceRange.baseArrayLayer = 0;
			barrier1.subresourceRange.baseMipLevel = 0;
			barrier1.subresourceRange.layerCount = 6;
			barrier1.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(m_cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr, 0, nullptr, 1, &barrier1);
		}
	}

	if (vkEndCommandBuffer(m_cb) != VK_SUCCESS)
		throw std::runtime_error("failed to record command buffer!");

	//submit cb
	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &m_cb;
	/*submit.pSignalSemaphores = &m_render_finished_s;
	submit.signalSemaphoreCount = 1;*/
	if (vkQueueSubmit(m_base.graphics_queue, 1, &submit, m_render_finished_f) != VK_SUCCESS)
		throw std::runtime_error("failed to submit cb!");
	vkWaitForFences(m_base.device, 1, &m_render_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_base.device, 1, &m_render_finished_f);
}

void omni_light_shadow_pass::create_semaphore()
{
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(m_base.device, &info, host_memory_manager, &m_render_finished_s) != VK_SUCCESS)
		throw std::runtime_error("failed to create semaphore!");
}

void omni_light_shadow_pass::create_framebuffer(light& l)
{
	VkFramebufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.attachmentCount = 1;
	info.height = SHADOW_MAP_SIZE;
	info.width = SHADOW_MAP_SIZE;
	info.layers = 6;
	info.pAttachments = &l.shadow_map.view;
	info.renderPass = m_pass;
	if (vkCreateFramebuffer(m_base.device, &info, host_memory_manager, &l.shadow_map_fb) != VK_SUCCESS)
		throw std::runtime_error("failed to create framebuffer!");
}

void omni_light_shadow_pass::create_fence()
{
	VkFenceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	
	if (vkCreateFence(m_base.device, &info, host_memory_manager, &m_render_finished_f) != VK_SUCCESS)
		throw std::runtime_error("failed to create fence!");
}