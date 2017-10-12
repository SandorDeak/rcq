#include "gta5_pass.h"

#include "resource_manager.h"

using namespace rcq;
using namespace rcq::gta5;

gta5_pass* gta5_pass::m_instance = nullptr;

gta5_pass::gta5_pass(const base_info& info) : m_base(info)
{
	send_memory_requirements();
	create_render_pass();
	create_descriptos_set_layouts();
	create_graphics_pipelines();

}


gta5_pass::~gta5_pass()
{
	for (auto& gp : m_gps)
	{
		vkDestroyPipeline(m_base.device, gp.gp, host_memory_manager);
		vkDestroyPipelineLayout(m_base.device, gp.pl, host_memory_manager);
		vkDestroyDescriptorSetLayout(m_base.device, gp.dsl, host_memory_manager);
	}
	vkDestroyRenderPass(m_base.device, m_pass, host_memory_manager);
}

void gta5_pass::init(const base_info& info)
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("gta5_pass is already initialised!");
	}
	m_instance = new gta5_pass(info);
}

void gta5_pass::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy gta5_pass, it doesn't exist!");
	}

	delete m_instance;
}

void gta5_pass::create_render_pass()
{
	//create attachments
	VkAttachmentDescription attachments[ATTACHMENT_COUNT];
	attachments[ATTACHMENT_GB_POS_ROUGHNESS].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[ATTACHMENT_GB_POS_ROUGHNESS].finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[ATTACHMENT_GB_POS_ROUGHNESS].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[ATTACHMENT_GB_POS_ROUGHNESS].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[ATTACHMENT_GB_POS_ROUGHNESS].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attachments[ATTACHMENT_GB_POS_ROUGHNESS].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[ATTACHMENT_GB_POS_ROUGHNESS].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[ATTACHMENT_GB_POS_ROUGHNESS].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	
	attachments[ATTACHMENT_GB_F0_SSAO] = attachments[ATTACHMENT_GB_POS_ROUGHNESS];

	attachments[ATTACHMENT_GB_ALBEDO_SSDS] = attachments[ATTACHMENT_GB_POS_ROUGHNESS];

	 attachments[ATTACHMENT_GB_NORMAL_AO] = attachments[ATTACHMENT_GB_POS_ROUGHNESS];

	attachments[ATTACHMENT_GB_DEPTH] = attachments[ATTACHMENT_GB_POS_ROUGHNESS];
	attachments[ATTACHMENT_GB_DEPTH].format = find_depth_format(m_base.physical_device);
	attachments[ATTACHMENT_GB_DEPTH].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[ATTACHMENT_GB_DEPTH].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

	attachments[ATTACHMENT_EM_COLOR] = attachments[ATTACHMENT_GB_POS_ROUGHNESS];

	attachments[ATTACHMENT_EM_DEPTH] = attachments[ATTACHMENT_GB_POS_ROUGHNESS];

	attachments[ATTACHMENT_DS_MAP] = attachments[ATTACHMENT_GB_POS_ROUGHNESS];
	attachments[ATTACHMENT_DS_MAP].format = VK_FORMAT_D32_SFLOAT;
	attachments[ATTACHMENT_DS_MAP].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

	attachments[ATTACHMENT_SSDS_MAP] = attachments[ATTACHMENT_GB_POS_ROUGHNESS];
	attachments[ATTACHMENT_SSDS_MAP].format = VK_FORMAT_D32_SFLOAT;


	attachments[ATTACHMENT_SSAO_MAP] = attachments[ATTACHMENT_SSDS_MAP];

	attachments[ATTACHMENT_PREIMAGE] = attachments[ATTACHMENT_GB_POS_ROUGHNESS];
	attachments[ATTACHMENT_PREIMAGE].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


	//create subpasses and references
	std::array<VkSubpassDescription, SUBPASS_COUNT> subpasses = {};

	//g-buffer generation subpass
	std::array<VkAttachmentReference, 4> gb_gen_refs_out;
	gb_gen_refs_out[0].attachment = ATTACHMENT_GB_POS_ROUGHNESS;
	gb_gen_refs_out[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gb_gen_refs_out[1].attachment = ATTACHMENT_GB_F0_SSAO;
	gb_gen_refs_out[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gb_gen_refs_out[2].attachment = ATTACHMENT_GB_ALBEDO_SSDS;
	gb_gen_refs_out[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gb_gen_refs_out[3].attachment = ATTACHMENT_GB_NORMAL_AO;
	gb_gen_refs_out[3].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference gb_gen_refs_depth;
	gb_gen_refs_depth.attachment = ATTACHMENT_GB_DEPTH;
	gb_gen_refs_depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	subpasses[SUBPASS_GBUFFER_GEN].colorAttachmentCount = gb_gen_refs_out.size();
	subpasses[SUBPASS_GBUFFER_GEN].pColorAttachments = gb_gen_refs_out.data();
	subpasses[SUBPASS_GBUFFER_GEN].pDepthStencilAttachment = &gb_gen_refs_depth;
	subpasses[SUBPASS_GBUFFER_GEN].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//environment map generation subpass
	VkAttachmentReference em_gen_refs_out;
	em_gen_refs_out.attachment = ATTACHMENT_EM_COLOR;
	em_gen_refs_out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference em_gen_refs_depth;
	em_gen_refs_depth.attachment = ATTACHMENT_EM_DEPTH;
	em_gen_refs_depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	subpasses[SUBPASS_ENVIRONMENT_MAP_GEN].colorAttachmentCount = 1;
	subpasses[SUBPASS_ENVIRONMENT_MAP_GEN].pColorAttachments = &em_gen_refs_out;
	subpasses[SUBPASS_ENVIRONMENT_MAP_GEN].pDepthStencilAttachment = &em_gen_refs_depth;
	subpasses[SUBPASS_ENVIRONMENT_MAP_GEN].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//dir shadow map generation subpass
	VkAttachmentReference dir_shadow_gen_refs_depth;
	dir_shadow_gen_refs_depth.attachment = ATTACHMENT_DS_MAP;
	dir_shadow_gen_refs_depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	subpasses[SUBPASS_DIR_SHADOW_MAP_GEN].pDepthStencilAttachment = &dir_shadow_gen_refs_depth;
	subpasses[SUBPASS_DIR_SHADOW_MAP_GEN].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//ss dir shadow map generation subpass
	VkAttachmentReference ss_dir_shadow_map_gen_refs_out;
	ss_dir_shadow_map_gen_refs_out.attachment = ATTACHMENT_SSDS_MAP;
	ss_dir_shadow_map_gen_refs_out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	
	VkAttachmentReference ss_dir_shadow_map_gen_refs_in;
	ss_dir_shadow_map_gen_refs_in.attachment = ATTACHMENT_GB_POS_ROUGHNESS;
	ss_dir_shadow_map_gen_refs_in.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_GEN].colorAttachmentCount = 1;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_GEN].inputAttachmentCount = 1;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_GEN].pColorAttachments = &ss_dir_shadow_map_gen_refs_out;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_GEN].pInputAttachments = &ss_dir_shadow_map_gen_refs_in;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_GEN].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//ss dir shadow map blur subpass
	std::array<VkAttachmentReference, 2> ss_dir_shadow_map_blur_refs_in;
	ss_dir_shadow_map_blur_refs_in[0].attachment = ATTACHMENT_GB_POS_ROUGHNESS;
	ss_dir_shadow_map_blur_refs_in[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ss_dir_shadow_map_blur_refs_in[1].attachment = ATTACHMENT_GB_NORMAL_AO;
	ss_dir_shadow_map_blur_refs_in[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	VkAttachmentReference ss_dir_shadow_map_blur_refs_out;
	ss_dir_shadow_map_blur_refs_out.attachment = ATTACHMENT_GB_ALBEDO_SSDS;
	ss_dir_shadow_map_blur_refs_out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_BLUR].colorAttachmentCount = 1;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_BLUR].inputAttachmentCount = 2;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_BLUR].pColorAttachments = &ss_dir_shadow_map_blur_refs_out;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_BLUR].pInputAttachments = ss_dir_shadow_map_blur_refs_in.data();
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_BLUR].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//ssao generation subpass
	std::array<VkAttachmentReference, 2> ssao_gen_refs_in;
	ssao_gen_refs_in[0].attachment = ATTACHMENT_GB_POS_ROUGHNESS;
	ssao_gen_refs_in[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ssao_gen_refs_in[1].attachment = ATTACHMENT_GB_NORMAL_AO;
	ssao_gen_refs_in[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference ssao_gen_refs_out;
	ssao_gen_refs_out.attachment = ATTACHMENT_SSAO_MAP;
	ssao_gen_refs_out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	subpasses[SUBPASS_SSAO_GEN].colorAttachmentCount = 1;
	subpasses[SUBPASS_SSAO_GEN].inputAttachmentCount = 2;
	subpasses[SUBPASS_SSAO_GEN].pColorAttachments = &ssao_gen_refs_out;
	subpasses[SUBPASS_SSAO_GEN].pInputAttachments = ssao_gen_refs_in.data();
	subpasses[SUBPASS_SSAO_GEN].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//ssao blur subpass
	VkAttachmentReference ssao_blur_refs_out;
	ssao_blur_refs_out.attachment = ATTACHMENT_GB_F0_SSAO;
	ssao_blur_refs_out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	subpasses[SUBPASS_SSAO_BLUR].colorAttachmentCount = 1;
	subpasses[SUBPASS_SSAO_BLUR].pColorAttachments = &ssao_blur_refs_out;
	subpasses[SUBPASS_SSAO_BLUR].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//image assembler subpass
	std::array<VkAttachmentReference, 4> image_assembler_refs_in;
	image_assembler_refs_in[0].attachment = ATTACHMENT_GB_POS_ROUGHNESS;
	image_assembler_refs_in[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_assembler_refs_in[1].attachment = ATTACHMENT_GB_F0_SSAO;
	image_assembler_refs_in[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_assembler_refs_in[2].attachment = ATTACHMENT_GB_ALBEDO_SSDS;
	image_assembler_refs_in[2].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_assembler_refs_in[3].attachment = ATTACHMENT_GB_NORMAL_AO;
	image_assembler_refs_in[3].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference image_assembler_refs_depth;
	image_assembler_refs_depth.attachment = ATTACHMENT_GB_DEPTH;
	image_assembler_refs_depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference image_assembler_refs_out;
	image_assembler_refs_out.attachment = ATTACHMENT_PREIMAGE;
	image_assembler_refs_out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	subpasses[SUBPASS_IMAGE_ASSEMBLER].colorAttachmentCount = 1;
	subpasses[SUBPASS_IMAGE_ASSEMBLER].inputAttachmentCount = 4;
	subpasses[SUBPASS_IMAGE_ASSEMBLER].pColorAttachments = &image_assembler_refs_out;
	subpasses[SUBPASS_IMAGE_ASSEMBLER].pInputAttachments = image_assembler_refs_in.data();
	subpasses[SUBPASS_IMAGE_ASSEMBLER].pDepthStencilAttachment = &image_assembler_refs_depth;

	//dependencies




	VkRenderPassCreateInfo pass_info = {};
	pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	pass_info.attachmentCount = ATTACHMENT_COUNT;

}

void gta5_pass::create_graphics_pipelines()
{
	std::array<graphics_pipeline_create_info, GP_COUNT> gps;
	for (auto& gp : gps)
		gp.set_device(m_base.device);

	std::array<VkGraphicsPipelineCreateInfo, GP_COUNT> infos = {};
	
	//emvironment map gen mat
	{
		auto& gp = gps[GP_ENVIRONMENT_MAP_GEN_MAT];
		auto& create = infos[GP_ENVIRONMENT_MAP_GEN_MAT];

		gp.create_shader_modules({
			"shaders/gta5_pass/environment_map_gen/environment_map_gen_mat/vert.spv",
			"shaders/gta5_pass/environment_map_gen/environment_map_gen_mat/geom.spv",
			"shaders/gta5_pass/environment_map_gen/environment_map_gen_mat/frag.spv"
		});
		gp.fill_shader_create_infos({ VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT });

		gp.add_vertex_binding(vertex::get_binding_description());
		constexpr auto attribs = vertex::get_attribute_descriptions();
		for (auto& attrib : attribs)
			gp.add_vertex_attrib(attrib);
		gp.set_vertex_input_pointers();
		
		gp.fill_assembly(VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		gp.set_default_viewport(ENVIRONMENT_MAP_SIZE, ENVIRONMENT_MAP_SIZE);
		gp.set_viewport_pointers();

		gp.fill_rasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE, 0.f, 0.f, 0.f);

		gp.fill_depthstencil(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE, {}, {}, 0.f, 0.f);
		
		gp.fill_multisample(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.f, 0.f, VK_FALSE, VK_FALSE);

		VkPipelineColorBlendAttachmentState att = {};
		att.blendEnable = VK_FALSE;
		att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		gp.add_color_attachment(att);
		gp.fill_blend(VK_FALSE, VK_LOGIC_OP_COPY);
		gp.set_blend_pointers();

		gp.add_dsl(m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].dsl);
		m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].pl=gp.create_layout();

		gp.fill_create_info(create, m_pass, SUBPASS_ENVIRONMENT_MAP_GEN, VK_NULL_HANDLE, -1);
		gp.set_create_info_pointers(create);
	}

	//environment map gen skybox
	{
		auto& gp = gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX];
		auto& create = infos[GP_ENVIRONMENT_MAP_GEN_SKYBOX];
		auto& pipeline = m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX];

		gp.create_shader_modules({
			"shaders/gta5_pass/environment_map_gen/environment_map_gen_skybox/vert.spv",
			"shaders/gta5_pass/environment_map_gen/environment_map_gen_skybox/geom.spv",
			"shaders/gta5_pass/environment_map_gen/environment_map_gen_skybox/frag.spv", });

		gp.fill_shader_create_infos({ VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT });

		gp.fill_assembly(VK_FALSE, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

		gp.set_default_viewport(ENVIRONMENT_MAP_SIZE, ENVIRONMENT_MAP_SIZE);

		gp.set_viewport_pointers();

		gp.fill_rasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_FALSE,
			0.f, 0.f, 0.f);

		gp.fill_depthstencil(VK_TRUE, VK_FALSE, VK_COMPARE_OP_EQUAL, VK_FALSE, VK_FALSE, {}, {}, 0.f, 1.f);

		gp.fill_multisample(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.f, 0, VK_FALSE, VK_FALSE);

		VkPipelineColorBlendAttachmentState att = {};
		att.blendEnable = VK_FALSE;
		att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		gp.add_color_attachment(att);
		gp.fill_blend(VK_FALSE, VK_LOGIC_OP_COPY);
		gp.set_blend_pointers();

		gp.add_dsl(pipeline.dsl);
		gp.add_dsl(resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX));
		pipeline.pl = gp.create_layout();

		gp.fill_create_info(create, m_pass, SUBPASS_ENVIRONMENT_MAP_GEN, VK_NULL_HANDLE, -1);
		gp.set_create_info_pointers(create);
	}

	//g-buffer gen
	{
		auto& gp = gps[GP_GBUFFER_GEN];
		auto& create = infos[GP_GBUFFER_GEN];
		auto& pipeline = m_gps[GP_GBUFFER_GEN];

		gp.create_shader_modules({
			"shaders/gta5_pass/gbuffer_gen/vert.spv",
			"shaders/gta5_pass/gbuffer_gen/frag.spv",
		});
		gp.fill_shader_create_infos({ VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT });

		constexpr auto bindings = get_vertex_input_binding_descriptions();
		constexpr auto attribs = get_vertex_input_attribute_descriptions();
		for (auto& binding : bindings)
			gp.add_vertex_binding(binding);
		for (auto& attrib : attribs)
			gp.add_vertex_attrib(attrib);
		gp.set_vertex_input_pointers();

		gp.fill_assembly(VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		gp.set_default_viewport(m_base.swap_chain_image_extent.width, m_base.swap_chain_image_extent.height);

		gp.set_viewport_pointers();

		gp.fill_rasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE, 0.f, 0.f, 0.f);

		gp.fill_depthstencil(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE, {}, {}, 0.f, 0.f);

		gp.fill_multisample(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.f, 0, VK_FALSE, VK_FALSE);

		VkPipelineColorBlendAttachmentState att = {};
		att.blendEnable = VK_FALSE;
		att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		for(uint32_t i=0; i<4; ++i)
			gp.add_color_attachment(att);
		
		gp.fill_blend(VK_FALSE, VK_LOGIC_OP_COPY);
		gp.set_blend_pointers();

		gp.add_dsl(pipeline.dsl);
		gp.add_dsl(resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_TR));
		gp.add_dsl(resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE));
		pipeline.pl = gp.create_layout();

		gp.fill_create_info(create, m_pass, SUBPASS_GBUFFER_GEN, VK_NULL_HANDLE, -1);
		gp.set_create_info_pointers(create);
	}

	//dir shadow map gen
	{
		auto& gp = gps[GP_DIR_SHADOW_MAP_GEN];
		auto& create = infos[GP_DIR_SHADOW_MAP_GEN];
		auto& pipeline = m_gps[GP_DIR_SHADOW_MAP_GEN];

		gp.create_shader_modules({
			"shaders/dir_shadow_map_gen/vert.spv",
			"shaders/dir_shadow_map_gen/geom.spv",
		});
		gp.fill_shader_create_infos({ VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_GEOMETRY_BIT });

		constexpr auto binding = vertex::get_binding_description();
		constexpr auto attrib = vertex::get_attribute_descriptions()[0];
		gp.add_vertex_binding(binding);
		gp.add_vertex_attrib(attrib);
		gp.set_vertex_input_pointers();

		gp.fill_assembly(VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		
		gp.set_default_viewport(DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE);
		gp.set_viewport_pointers();

		gp.fill_rasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE, 0.f, 0.f, 0.f);
		
		gp.fill_depthstencil(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE, {}, {}, 0.f, 1.f);

		gp.fill_multisample(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.f, 0, VK_FALSE, VK_FALSE);
		
		gp.add_dsl(pipeline.dsl);
		gp.add_dsl(resource_manager::instance()->get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE_TR));
		pipeline.pl = gp.create_layout();

		gp.fill_create_info(create, m_pass, SUBPASS_DIR_SHADOW_MAP_GEN, VK_NULL_HANDLE, -1);
		gp.set_create_info_pointers(create);
	}

	//ss dir shadow map gen
	{
		auto& gp = gps[GP_SS_DIR_SHADOW_MAP_GEN];
		auto& create = infos[GP_SS_DIR_SHADOW_MAP_GEN];
		auto& pipeline = m_gps[GP_SS_DIR_SHADOW_MAP_GEN];

		gp.create_shader_modules({
			"shaders/ss_dir_shadow_map_gen/vert.spv",
			"shaders/ss_dir_shadow_map_gen/frag.spv"
		});

		gp.fill_assembly(VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);

		gp.set_default_viewport(m_base.swap_chain_image_extent.width, m_base.swap_chain_image_extent.height);
		gp.set_viewport_pointers();

		gp.fill_rasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_FALSE,
			0.f, 0.f, 0.f);

		gp.fill_multisample(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.f, 0, VK_FALSE, VK_FALSE);

		VkPipelineColorBlendAttachmentState att = {};
		att.blendEnable = VK_FALSE;
		att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
		gp.add_color_attachment(att);
		gp.fill_blend(VK_FALSE, VK_LOGIC_OP_COPY);
		gp.set_blend_pointers();
		
		gp.add_dsl(pipeline.dsl);
		pipeline.pl = gp.create_layout();

		gp.fill_create_info(create,m_pass, SUBPASS_SS_DIR_SHADOW_MAP_GEN, VK_NULL_HANDLE, -1);
		gp.set_create_info_pointers(create);
	}

	//ss dir shadow map blur
	{
		auto& gp = gps[GP_SS_DIR_SHADOW_MAP_BLUR];
		auto& create = infos[GP_SS_DIR_SHADOW_MAP_BLUR];
		auto& pipeline = m_gps[GP_SS_DIR_SHADOW_MAP_BLUR];

		gp.create_shader_modules({
			"shaders/gta5_pass/ss_dir_shadow_map_blur/vert.spv",
			"shaders/gta5_pass/ss_dir_shadow_map_blur/frag.spv",
		});
		gp.fill_shader_create_infos({ VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT });

		gp.fill_assembly(VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);

		gp.set_default_viewport(m_base.swap_chain_image_extent.width, m_base.swap_chain_image_extent.height);
		gp.set_viewport_pointers();

		gp.fill_rasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_FALSE,
			0.f, 0.f, 0.f);

		gp.fill_multisample(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.f, 0, VK_FALSE, VK_FALSE);
		
		VkPipelineColorBlendAttachmentState att = {};
		att.blendEnable = VK_FALSE;
		att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
		gp.add_color_attachment(att);
		gp.fill_blend(VK_FALSE, VK_LOGIC_OP_COPY);
		gp.set_blend_pointers();

		gp.add_dsl(pipeline.dsl);
		pipeline.pl = gp.create_layout();

		gp.fill_create_info(create, m_pass, SUBPASS_SS_DIR_SHADOW_MAP_BLUR, VK_NULL_HANDLE, -1);
		gp.set_create_info_pointers(create);
	}
		
	//ssao gen
	{
		auto& gp = gps[GP_SSAO_GEN];
		auto& create = infos[GP_SSAO_GEN];
		auto& pipeline = m_gps[GP_SSAO_GEN];

		gp.create_shader_modules({
			"shaders/gta5_pass/ssao_gen/vert.spv",
			"shaders/gta5_pass/ssao_gen/frag.spv"
		});
		gp.fill_shader_create_infos({ VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT });

		gp.fill_assembly(VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);

		gp.set_default_viewport(m_base.swap_chain_image_extent.width, m_base.swap_chain_image_extent.height);
		gp.set_viewport_pointers();

		gp.fill_rasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_FALSE,
			0.f, 0.f, 0.f);

		gp.fill_multisample(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.f, 0, VK_FALSE, VK_FALSE);

		VkPipelineColorBlendAttachmentState att = {};
		att.blendEnable = VK_FALSE;
		att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
		gp.add_color_attachment(att);
		gp.fill_blend(VK_FALSE, VK_LOGIC_OP_COPY);
		gp.set_blend_pointers();

		gp.add_dsl(pipeline.dsl);
		pipeline.pl = gp.create_layout();

		gp.fill_create_info(create, m_pass, SUBPASS_SSAO_GEN, VK_NULL_HANDLE, -1);
		gp.set_create_info_pointers(create);	
	}

	//ssao blur
	{
		auto& gp = gps[GP_SSAO_BLUR];
		auto& create = infos[GP_SSAO_BLUR];
		auto& pipeline = m_gps[GP_SSAO_BLUR];

		gp.create_shader_modules({
			"shaders/gta5_pass/ssao_blur/vert.spv",
			"shaders/gta5_pass/ssao_blur/frag.spv"
		});
		gp.fill_shader_create_infos({ VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT });

		gp.fill_assembly(VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);

		gp.set_default_viewport(m_base.swap_chain_image_extent.width, m_base.swap_chain_image_extent.height);
		gp.set_viewport_pointers();

		gp.fill_rasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_FALSE,
			0.f, 0.f, 0.f);

		gp.fill_multisample(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.f, 0, VK_FALSE, VK_FALSE);

		VkPipelineColorBlendAttachmentState att = {};
		att.blendEnable = VK_FALSE;
		att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
		gp.add_color_attachment(att);
		gp.fill_blend(VK_FALSE, VK_LOGIC_OP_COPY);
		gp.set_blend_pointers();

		gp.add_dsl(pipeline.dsl);
		pipeline.pl = gp.create_layout();

		gp.fill_create_info(create, m_pass, SUBPASS_SSAO_BLUR, VK_NULL_HANDLE, -1);
		gp.set_create_info_pointers(create);
	}

	//image assembler
	{
		auto& gp = gps[GP_IMAGE_ASSEMBLER];
		auto& create = infos[GP_IMAGE_ASSEMBLER];
		auto& pipeline = m_gps[GP_IMAGE_ASSEMBLER];

		gp.create_shader_modules({
			"shaders/gta5_pass/image_assembler/vert.spv",
			"shaders/gta5_pass/image_assembler/frag.spv"
		});
		gp.fill_shader_create_infos({ VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT });

		gp.fill_assembly(VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);

		gp.set_default_viewport(m_base.swap_chain_image_extent.width, m_base.swap_chain_image_extent.height);
		gp.set_viewport_pointers();

		gp.fill_rasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_FALSE,
			0.f, 0.f, 0.f);

		gp.fill_multisample(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.f, 0, VK_FALSE, VK_FALSE);

		VkPipelineColorBlendAttachmentState att = {};
		att.blendEnable = VK_FALSE;
		att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
		gp.add_color_attachment(att);
		gp.fill_blend(VK_FALSE, VK_LOGIC_OP_COPY);
		gp.set_blend_pointers();

		gp.add_dsl(pipeline.dsl);
		pipeline.pl = gp.create_layout();

		gp.fill_create_info(create, m_pass, SUBPASS_IMAGE_ASSEMBLER, VK_NULL_HANDLE, -1);
		gp.set_create_info_pointers(create);
	}

	std::array<VkPipeline, GP_COUNT> pipelines;
	if (vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, GP_COUNT, infos.data(), host_memory_manager, pipelines.data()) 
		!= VK_SUCCESS)
		throw std::runtime_error("failed to create gta5 pipelines!");

	for (auto& gp : gps)
		gp.destroy_modules();
	for (uint32_t i = 0; i < GP_COUNT; ++i)
	{
		m_gps[i].gp = pipelines[i];
	}
}

void gta5_pass::create_descriptos_set_layouts()
{
	//environment map gen mat
	{
		VkDescriptorSetLayoutBinding binding_data = {};
		binding_data.binding = 0;
		binding_data.descriptorCount = 1;
		binding_data.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding_data.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = &binding_data;

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, host_memory_manager, &m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].dsl) != VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//environment map gen skybox
	{
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorCount = 1;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = &binding;

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, host_memory_manager, &m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].dsl) 
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//gbuffer gen
	{
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorCount = 1;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = &binding;

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, host_memory_manager, &m_gps[GP_GBUFFER_GEN].dsl)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//dir shadow map gen
	{
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorCount = 1;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = &binding;

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, host_memory_manager, &m_gps[GP_DIR_SHADOW_MAP_GEN].dsl)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//ss dir shadow map gen
	{
		std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[2].binding = 2;
		bindings[2].descriptorCount = 1;
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 3;
		dsl.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, host_memory_manager, &m_gps[GP_SS_DIR_SHADOW_MAP_GEN].dsl)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//ss dir shadow map blur
	{
		std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};
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
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 3;
		dsl.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, host_memory_manager, &m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].dsl)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//ssao gen
	{
		std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 2;
		dsl.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, host_memory_manager, &m_gps[GP_SSAO_GEN].dsl)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//ssao blur
	{
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorCount = 1;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = &binding;

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, host_memory_manager, &m_gps[GP_SSAO_BLUR].dsl)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}

	//image assembler
	{
		std::array<VkDescriptorSetLayoutBinding, 8> bindings = {};

		for (uint32_t i = 0; i < 6; ++i)
		{
			bindings[i].binding = 0;
			bindings[i].descriptorCount = 1;
			bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		bindings[6].binding = 6;
		bindings[6].descriptorCount = 1;
		bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[7].binding = 6;
		bindings[7].descriptorCount = 1;
		bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 8;
		dsl.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(m_base.device, &dsl, host_memory_manager, &m_gps[GP_IMAGE_ASSEMBLER].dsl)
			!= VK_SUCCESS)
			throw std::runtime_error("failed to create dsl!");
	}
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
		vkGetBufferMemoryRequirements(m_base.device, m_res_data.staging_buffer, &mr);

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
		image.extent.height = SHADOW_MAP_SIZE;
		image.extent.width = SHADOW_MAP_SIZE;
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
		image.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

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
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

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
	std::get<RESOURCE_TYPE_MEMORY>(package).emplace_back(RENDER_PASS_GTA5, std::move(alloc_infos));

	resource_manager::instance()->process_build_package(std::move(package));
}

void rcq::gta5::gta5_pass::get_memory_and_build_resources()
{
	memory mem = resource_manager::instance()->get<RESOURCE_TYPE_MEMORY>(RENDER_PASS_GTA5);

	//staging buffer
	{
		m_res_data.staging_buffer_mem = mem[MEMORY_STAGING_BUFFER];
		vkBindBufferMemory(m_base.device, m_res_data.staging_buffer_mem, m_res_data.staging_buffer_mem, 0);

		void* raw_data;
		vkMapMemory(m_base.device, m_res_data.staging_buffer_mem, 0, m_res_data.size, 0, &raw_data);
		char* data = static_cast<char*>(raw_data);
		m_res_data.set_pointers(data, std::make_index_sequence<RES_DATA_COUNT>());
	}

	//buffer
	{
		m_res_data.buffer_mem = mem[MEMORY_BUFFER];
		vkBindBufferMemory(m_base.device, m_res_data.buffer, m_res_data.buffer_mem, 0);
	}

	//environment map gen depthstencil
	{
		m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].memory = mem[MEMORY_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL];
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].image,
			m_res_image[RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL].memory, 0);

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
		m_res_image[RES_IMAGE_ENVIRONMENT_MAP].memory = mem[MEMORY_ENVIRONMENT_MAP];
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image,
			m_res_image[RES_IMAGE_ENVIRONMENT_MAP].memory, 0);

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
		m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].memory = mem[MEMORY_GB_POS_ROUGHNESS];
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image,
			m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].memory, 0);

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
		m_res_image[RES_IMAGE_GB_F0_SSAO].memory = mem[MEMORY_GB_F0_SSAO];
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_F0_SSAO].image,
			m_res_image[RES_IMAGE_GB_F0_SSAO].memory, 0);

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
		m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].memory = mem[MEMORY_GB_ALBEDO_SSDS];
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].image,
			m_res_image[RES_IMAGE_GB_ALBEDO_SSDS].memory, 0);

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
		m_res_image[RES_IMAGE_GB_NORMAL_AO].memory = mem[MEMORY_GB_NORMAL_AO];
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_NORMAL_AO].image,
			m_res_image[RES_IMAGE_GB_NORMAL_AO].memory, 0);

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

	//gbuffer depth
	{
		m_res_image[RES_IMAGE_GB_DEPTH].memory = mem[MEMORY_GB_DEPTH];
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_GB_DEPTH].image,
			m_res_image[RES_IMAGE_GB_DEPTH].memory, 0);

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
		m_res_image[RES_IMAGE_DIR_SHADOW_MAP].memory = mem[MEMORY_DIR_SHADOW_MAP];
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image,
			m_res_image[RES_IMAGE_DIR_SHADOW_MAP].memory, 0);

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.format = VK_FORMAT_D32_SFLOAT;
		view.image = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
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
		m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].memory = mem[MEMORY_SS_DIR_SHADOW_MAP];
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].image,
			m_res_image[RES_IMAGE_SS_DIR_SHADOW_MAP].memory, 0);

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
		m_res_image[RES_IMAGE_SSAO_MAP].memory = mem[MEMORY_SSAO_MAP];
		vkBindImageMemory(m_base.device, m_res_image[RES_IMAGE_SSAO_MAP].image,
			m_res_image[RES_IMAGE_SSAO_MAP].memory, 0);

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

	//transition images to proper layout
	{

	}

}