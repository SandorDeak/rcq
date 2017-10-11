#include "gta5_pass.h"

using namespace rcq;

gta5_pass* gta5_pass::m_instance = nullptr;

gta5_pass::gta5_pass(const base_info& info) : m_base(info)
{
}


gta5_pass::~gta5_pass()
{
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
	attachments[ATTACHMENT_GB_POS].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[ATTACHMENT_GB_POS].finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[ATTACHMENT_GB_POS].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[ATTACHMENT_GB_POS].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[ATTACHMENT_GB_POS].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attachments[ATTACHMENT_GB_POS].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[ATTACHMENT_GB_POS].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[ATTACHMENT_GB_POS].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	
	attachments[ATTACHMENT_GB_F0_ROUGHNESS] = attachments[ATTACHMENT_GB_POS];

	attachments[ATTACHMENT_GB_ALBEDO_AO] = attachments[ATTACHMENT_GB_POS];

	 attachments[ATTACHMENT_GB_NORMAL] = attachments[ATTACHMENT_GB_POS];

	attachments[ATTACHMENT_GB_DEPTH] = attachments[ATTACHMENT_GB_POS];
	attachments[ATTACHMENT_GB_DEPTH].format = find_depth_format(m_base.physical_device);
	attachments[ATTACHMENT_GB_DEPTH].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[ATTACHMENT_GB_DEPTH].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

	attachments[ATTACHMENT_EM_COLOR] = attachments[ATTACHMENT_GB_POS];

	attachments[ATTACHMENT_EM_DEPTH] = attachments[ATTACHMENT_GB_DEPTH];

	attachments[ATTACHMENT_DS_MAP] = attachments[ATTACHMENT_GB_POS];
	attachments[ATTACHMENT_DS_MAP].format = VK_FORMAT_D32_SFLOAT;
	attachments[ATTACHMENT_DS_MAP].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

	attachments[ATTACHMENT_SSDS_MAP] = attachments[ATTACHMENT_GB_POS];
	attachments[ATTACHMENT_SSDS_MAP].format = VK_FORMAT_D32_SFLOAT;


	attachments[ATTACHMENT_SSAO_MAP] = attachments[ATTACHMENT_SSDS_MAP];

	attachments[ATTACHMENT_PREIMAGE] = attachments[ATTACHMENT_GB_POS];
	attachments[ATTACHMENT_PREIMAGE].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	attachments[ATTACHMENT_SSDS_BLURRED_MAP] = attachments[ATTACHMENT_SSDS_MAP];

	attachments[ATTACHMENT_SSAO_BLURRED_MAP] = attachments[ATTACHMENT_SSAO_MAP];


	//create subpasses and references
	std::array<VkSubpassDescription, SUBPASS_COUNT> subpasses = {};

	//g-buffer generation subpass
	std::array<VkAttachmentReference, 4> gb_gen_refs_out;
	gb_gen_refs_out[0].attachment = ATTACHMENT_GB_POS;
	gb_gen_refs_out[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gb_gen_refs_out[1].attachment = ATTACHMENT_GB_F0_ROUGHNESS;
	gb_gen_refs_out[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gb_gen_refs_out[2].attachment = ATTACHMENT_GB_ALBEDO_AO;
	gb_gen_refs_out[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gb_gen_refs_out[3].attachment = ATTACHMENT_GB_NORMAL;
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
	ss_dir_shadow_map_gen_refs_in.attachment = ATTACHMENT_GB_POS;
	ss_dir_shadow_map_gen_refs_in.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_GEN].colorAttachmentCount = 1;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_GEN].inputAttachmentCount = 1;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_GEN].pColorAttachments = &ss_dir_shadow_map_gen_refs_out;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_GEN].pInputAttachments = &ss_dir_shadow_map_gen_refs_in;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_GEN].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//ss dir shadow map blur subpass
	std::array<VkAttachmentReference, 2> ss_dir_shadow_map_blur_refs_in;
	ss_dir_shadow_map_blur_refs_in[0].attachment = ATTACHMENT_GB_POS;
	ss_dir_shadow_map_blur_refs_in[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ss_dir_shadow_map_blur_refs_in[1].attachment = ATTACHMENT_GB_NORMAL;
	ss_dir_shadow_map_blur_refs_in[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	VkAttachmentReference ss_dir_shadow_map_blur_refs_out;
	ss_dir_shadow_map_blur_refs_out.attachment = ATTACHMENT_SSDS_BLURRED_MAP;
	ss_dir_shadow_map_blur_refs_out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_BLUR].colorAttachmentCount = 1;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_BLUR].inputAttachmentCount = 2;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_BLUR].pColorAttachments = &ss_dir_shadow_map_blur_refs_out;
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_BLUR].pInputAttachments = ss_dir_shadow_map_blur_refs_in.data();
	subpasses[SUBPASS_SS_DIR_SHADOW_MAP_BLUR].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//ssao generation subpass
	std::array<VkAttachmentReference, 2> ssao_gen_refs_in;
	ssao_gen_refs_in[0].attachment = ATTACHMENT_GB_POS;
	ssao_gen_refs_in[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ssao_gen_refs_in[1].attachment = ATTACHMENT_GB_NORMAL;
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
	ssao_blur_refs_out.attachment = ATTACHMENT_SSDS_BLURRED_MAP;
	ssao_blur_refs_out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	subpasses[SUBPASS_SSAO_BLUR].colorAttachmentCount = 1;
	subpasses[SUBPASS_SSAO_BLUR].pColorAttachments = &ssao_blur_refs_out;
	subpasses[SUBPASS_SSAO_BLUR].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//image assembler subpass
	std::array<VkAttachmentReference, 6> image_assembler_refs_in;
	image_assembler_refs_in[0].attachment = ATTACHMENT_GB_POS;
	image_assembler_refs_in[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_assembler_refs_in[1].attachment = ATTACHMENT_GB_F0_ROUGHNESS;
	image_assembler_refs_in[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_assembler_refs_in[2].attachment = ATTACHMENT_GB_ALBEDO_AO;
	image_assembler_refs_in[2].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_assembler_refs_in[3].attachment = ATTACHMENT_GB_NORMAL;
	image_assembler_refs_in[3].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_assembler_refs_in[4].attachment = ATTACHMENT_SSDS_BLURRED_MAP;
	image_assembler_refs_in[4].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_assembler_refs_in[5].attachment = ATTACHMENT_SSAO_BLURRED_MAP;
	image_assembler_refs_in[5].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference image_assembler_refs_depth;
	image_assembler_refs_depth.attachment = ATTACHMENT_GB_DEPTH;
	image_assembler_refs_depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference image_assembler_refs_out;
	image_assembler_refs_out.attachment = ATTACHMENT_PREIMAGE;
	image_assembler_refs_out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	subpasses[SUBPASS_IMAGE_ASSEMBLER].colorAttachmentCount = 1;
	subpasses[SUBPASS_IMAGE_ASSEMBLER].inputAttachmentCount = 6;
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
	
	//emvironment map gen mat
	{
		auto& gp = gps[GP_ENVIRONMENT_MAP_GEN_MAT];

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
		
		gp.set_assembly(VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		gp.set_default_viewport(ENVIRONMENT_MAP_SIZE, ENVIRONMENT_MAP_SIZE);
		gp.set_viewport_pointers();

		gp.set_rasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE, 0.f, 0.f, 0.f);

		gp.set_depthstencil(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE, {}, {}, 0.f, 0.f);
		
		gp.set_multisample(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.f, 0.f, VK_FALSE, VK_FALSE);

		VkPipelineColorBlendAttachmentState att = {};
		att.blendEnable = VK_FALSE;
		att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		gp.add_color_attachment(att);
		gp.set_blend(VK_FALSE, VK_LOGIC_OP_COPY);
		gp.set_blend_pointers();

		gp.set_create_info(m_pass, SUBPASS_ENVIRONMENT_MAP_GEN, VK_NULL_HANDLE, -1);
		gp.set_create_info_pointers();
	}

	//environment map gen skybox
	{

	}
}
