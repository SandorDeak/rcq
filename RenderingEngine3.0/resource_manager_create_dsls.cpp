#include "resource_manager.h"

using namespace rcq;

void resource_manager::create_dsls()
{
	//create transform dsl
	{
		VkDescriptorSetLayoutBinding data = {};
		data.binding = 0;
		data.descriptorCount = 1;
		data.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		data.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = &data;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_TR]) == VK_SUCCESS);
	}

	//create material opaque dsl
	{
		VkDescriptorSetLayoutBinding bindings[TEX_TYPE_COUNT + 1] = {};
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		for (size_t i = 1; i < TEX_TYPE_COUNT + 1; ++i)
		{
			bindings[i].binding = i;
			bindings[i].descriptorCount = 1;
			bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = TEX_TYPE_COUNT + 1;
		dsl.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_MAT_OPAQUE]) == VK_SUCCESS);
	}

	//create sky dsl
	{
		VkDescriptorSetLayoutBinding bindings[3] = {};
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
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 3;
		dsl.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_SKY]) == VK_SUCCESS);
	}

	//create terrain dsl
	{
		VkDescriptorSetLayoutBinding bindings[2] = {};
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
			VK_SHADER_STAGE_VERTEX_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

		/*bindings[2].binding = 2;
		bindings[2].descriptorCount = 1;
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		bindings[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;*/

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 2;
		dsl.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_TERRAIN]) == VK_SUCCESS);
	}

	//create terrain compute dsl
	{
		VkDescriptorSetLayoutBinding bindings[1] = {};
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		/*bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;*/

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_TERRAIN_COMPUTE]) == VK_SUCCESS);
	}

	//create water dsl
	{
		VkDescriptorSetLayoutBinding bindings[1] = {};
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo dsl = {};
		dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl.bindingCount = 1;
		dsl.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl, m_vk_alloc, &m_dsls[DSL_TYPE_WATER]) == VK_SUCCESS);
	}

	//create water compute
	{
		VkDescriptorSetLayoutBinding bindings[3] = {};
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		bindings[2].binding = 2;
		bindings[2].descriptorCount = 1;
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutCreateInfo dsl_info = {};
		dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsl_info.bindingCount = 3;
		dsl_info.pBindings = bindings;

		assert(vkCreateDescriptorSetLayout(m_base.device, &dsl_info, m_vk_alloc, &m_dsls[DSL_TYPE_WATER_COMPUTE]) == VK_SUCCESS);
	}
}