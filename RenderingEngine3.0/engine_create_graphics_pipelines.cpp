#include "engine.h"
#include "utility.h"
#include "resource_manager.h"

#include "gps.h"

using namespace rcq;

template<uint32_t gp_id>
void engine::prepare_gp_create_info(VkGraphicsPipelineCreateInfo& create_info, VkPipelineLayoutCreateInfo& layout,
	VkPipelineShaderStageCreateInfo* shaders, VkShaderModuleCreateInfo* shader_modules, uint32_t& shader_index,
	VkDescriptorSetLayout* dsls, uint32_t& dsl_index,
	char* code, uint32_t& code_index)
{
	//fill create info
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;
	create_info.renderPass = m_rps[gp_create_info<gp_id>::render_pass];
	create_info.pStages = &shaders[shader_index];
	create_info.stageCount = gp_create_info<gp_id>::shader_filenames.size();
	create_info.pInputAssemblyState = &gp_create_info<gp_id>::input_assembly;
	create_info.pMultisampleState = &gp_create_info<gp_id>::multisample;
	create_info.pRasterizationState = &gp_create_info<gp_id>::rasterizer;
	create_info.pVertexInputState = &gp_create_info<gp_id>::vertex_input;
	create_info.pViewportState = &gp_create_info<gp_id>::viewport;
	gp_create_info<gp_id>::fill_optional(create_info);


	//fill shaders
	for (uint32_t i = 0; i < gp_create_info<gp_id>::shader_filenames.size(); ++i)
	{
		uint32_t size;
		utility::read_file(gp_create_info<gp_id>::shader_filenames[i], &code[code_index], size);
		shader_modules[shader_index].sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shader_modules[shader_index].codeSize = size;
		shader_modules[shader_index].pCode = reinterpret_cast<uint32_t*>(&code[code_index]);

		shaders[shader_index].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaders[shader_index].pName = "main";
		shaders[shader_index].stage = gp_create_info<gp_id>::shader_flags[i];

		code_index += size;
		++shader_index;
	}

	//create dsl
	assert(vkCreateDescriptorSetLayout(m_base.device, &gp_create_info<gp_id>::dsl::create_info, m_vk_alloc, &m_gps[gp_id].dsl)
		== VK_SUCCESS);

	//fill layout
	layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout.pSetLayouts = &dsls[dsl_index];
	layout.setLayoutCount = gp_create_info<gp_id>::dsl_types.size()+1;
	dsls[dsl_index++] = m_gps[gp_id].dsl;
	for (auto dsl_type : gp_create_info<gp_id>::dsl_types)
	{
		dsls[dsl_index++] = resource_manager::instance()->get_dsl(dsl_type);
	}
}

template<uint32_t... gp_ids>
void engine::prepare_gp_create_infos(std::index_sequence<gp_ids...>, 
	VkGraphicsPipelineCreateInfo* create_infos, VkPipelineLayoutCreateInfo* layouts,
	VkPipelineShaderStageCreateInfo* shaders, VkShaderModuleCreateInfo* shader_modules, uint32_t& shader_index,
	VkDescriptorSetLayout* dsls, uint32_t& dsl_index,
	char* code, uint32_t& code_index)
{
	auto l = { (prepare_gp_create_info<gp_ids>(create_infos[gp_ids], layouts[gp_ids], shaders, shader_modules, shader_index,
		dsls, dsl_index, code, code_index), 0)... };
}

void engine::create_graphics_pipelines()
{
	constexpr uint32_t  SHADER_COUNT = GP_COUNT * 3;
	constexpr uint32_t DSL_COUNT = GP_COUNT * 3;
	constexpr uint32_t CREATE_INFO_COUNT = GP_COUNT;
	constexpr uint32_t LAYOUT_COUNT = GP_COUNT;
	constexpr uint32_t CODE_SIZE = 256 * 1024;

	VkGraphicsPipelineCreateInfo create_infos[CREATE_INFO_COUNT] = {};
	VkPipelineShaderStageCreateInfo shaders[SHADER_COUNT] = {};
	VkShaderModuleCreateInfo shader_modules[SHADER_COUNT] = {};
	VkPipelineLayoutCreateInfo layouts[LAYOUT_COUNT] = {};
	VkDescriptorSetLayout dsls[DSL_COUNT];
	char code[CODE_SIZE];

	uint32_t shader_index = 0;
	uint32_t dsl_index = 0;
	uint32_t code_index = 0;

	prepare_gp_create_infos(std::make_index_sequence<GP_COUNT>(), create_infos, layouts, shaders, shader_modules, shader_index, 
		dsls, dsl_index, code, code_index);

	assert(shader_index <= SHADER_COUNT && dsl_index <= DSL_COUNT && code_index <= CODE_SIZE);

	//create layouts
	for(uint32_t i=0; i<GP_COUNT; ++i)
	{
		assert(vkCreatePipelineLayout(m_base.device, layouts + i, m_vk_alloc, &m_gps[i].pl) == VK_SUCCESS);
		create_infos[i].layout = m_gps[i].pl;
	}

	//create shader modules
	for (uint32_t i = 0; i < shader_index; ++i)
	{
		assert(vkCreateShaderModule(m_base.device, shader_modules + i, m_vk_alloc, &shaders[i].module) == VK_SUCCESS);
	}

	//create graphics pipelines
	VkPipeline gps[GP_COUNT];
	assert(vkCreateGraphicsPipelines(m_base.device, VK_NULL_HANDLE, GP_COUNT, create_infos, m_vk_alloc, gps) == VK_SUCCESS);

	for (uint32_t i = 0; i < GP_COUNT; ++i)
		m_gps[i].ppl = gps[i];

	//destroy shader modules
	for (uint32_t i = 0; i < shader_index; ++i)
		vkDestroyShaderModule(m_base.device, shaders[i].module, m_vk_alloc);
}