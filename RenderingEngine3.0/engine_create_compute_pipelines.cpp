#include "engine.h"
#include "utility.h"

#include "cps.h"

using namespace rcq;

template<size_t cp_id>
void engine::prepare_cp_create_info(VkComputePipelineCreateInfo& create_info, VkPipelineLayoutCreateInfo& layout,
	VkShaderModuleCreateInfo& shader_module,
	VkDescriptorSetLayout* dsls, uint32_t& dsl_index,
	char* code, uint32_t code_index)
{
	//fill create info
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	//fill shader
	uint32_t size;
	utility::read_file(cp_create_info<cp_id>::shader_filename, &code[code_index], size);
	shader_module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module.codeSize = size;
	create_info.stage.pCode = reinterpret_cast<uint32_t*>(&code[code_index]);
	code_index += size;

	create_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	create_info.stage.pName = "main";
	create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;

	//create dsl
	assert(vkCreateDescriptorSetLayout(m_base.device, &cp_create_info<cp_id>::dsl::create_info, m_vk_alloc, &m_cps[cp_id].dsl)
		== VK_SUCCESS);

	//fill layout
	layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout.pSetLayouts = &dsls[dsl_index];
	layout.setLayoutCount = cp_create_info<cp_id>::dsl_types.size();
	dsls[dsl_index++] = m_cps[cp_id].dsl;
	for (auto dsl_type : gp_create_info<cp_id>::dsl_types)
	{
		dsls[dsl_index++] = resource_manager::instance()->get_dsl(dsl_type);
	}
	layout.pushConstantRangeCount = cp_create_info<cp_id>::push_consts.size();
	layout.pPushConstantRanges = cp_create_info<cp_id>::push_consts.data();
}

template<uint32_t... cp_ids>
void engine::prepare_cp_create_infos(std::index_sequence<cp_ids...>,
	VkComputePipelineCreateInfo* create_infos, VkPipelineLayoutCreateInfo* layouts,
	VkShaderModuleCreateInfo* shader_modules,
	VkDescriptorSetLayout* dsls, uint32_t& dsl_index,
	char* code, uint32_t code_index)
{
	auto l = { (prepare_cp_create_info<cp_ids>(create_infos[cp_ids], layouts[cp_ids], shader_modules,
		dsls, dsl_index, code_index), 0), ... };
}

void engine::create_compute_pipelines()
{
	constexpr uint32_t CODE_SIZE = 64 * 1024;
	constexpr uint32_t DSL_SIZE = 5 * CP_COUNT;

	VkComputePipelineCreateInfo create_infos[CP_COUNT];
	VkPipelineLayoutCreateInfo layouts[CP_COUNT];
	VkDescriptorSetLayout dsls[DSL_SIZE];
	VkShaderModuleCreateInfo shader_modules[CP_COUNT] = {};
	char code[CODE_SIZE];

	uint32_t dsl_index = 0;
	uint32_t code_index = 0;

	prepare_cp_create_infos(std::make_index_sequence<CP_COUNT>(), create_infos, layouts, shader_modules,
		dsls, dsl_index, code, code_index);

	assert(dsl_index <= DSL_SIZE && code_index <= CODE_SIZE);

	//create layouts
	for (uint32_t i = 0; i<CP_COUNT; ++i)
	{
		assert(vkCreatePipelineLayout(m_base.device, layouts + i, m_vk_alloc, &m_cps[i].pl) == VK_SUCCESS);
		create_infos[i].layout = m_cps[i].pl;
	}

	//create shader modules
	for (uint32_t i = 0; i < CP_COUNT; ++i)
	{
		assert(vkCreateShaderModule(m_base.device, shader_modules + i, m_vk_alloc, &create_infos[i].stage.module) == VK_SUCCESS);
	}

	//create cps
	VkPipeline cps[CP_COUNT];
	assert(vkCreateComputePipelines(m_base.device, VK_NULL_HANDLE, CP_COUNT, create_infos, m_vk_alloc, cps) == VK_SUCCESS);

	for (uint32_t i = 0; i < CP_COUNT; ++i)
		m_cps[i].ppl = cps[i];

	//destroy shader modules
	for (uint32_t i = 0; i < CP_COUNT; ++i)
		vkDestroyShaderModule(m_base.device, create_infos[i].stage.module, m_vk_alloc);
}