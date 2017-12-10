#pragma once

#include "cp_create_info.h"
#include <array>

namespace rcq
{
	template<>
	struct cp_create_info<CP_BLOOM>
	{
		static constexpr const char* shader_filename = "shaders/gta5_pass/bloom_blur/comp.spv";
		static constexpr std::array<DSL_TYPE, 0> dsl_types = {};
		static constexpr std::array<VkPushConstantRange, 1> push_consts =
		{
			VK_SHADER_STAGE_COMPUTE_BIT,
			0,
			sizeof(uint32_t)
		};

		struct dsl
		{
			static constexpr std::array<VkDescriptorSetLayoutBinding, 1> bindings =
			{
				0,
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				1,
				VK_SHADER_STAGE_COMPUTE_BIT,
				nullptr
			};
			static constexpr VkDescriptorSetLayoutCreateInfo create_info
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				nullptr,
				0,
				bindings.size(),
				bindings.data()
			};
		};
	};
}