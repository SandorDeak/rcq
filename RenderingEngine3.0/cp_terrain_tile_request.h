#pragma once

#include "cp_create_info.h"
#include <array>

namespace rcq
{
	template<>
	struct cp_create_info<CP_TERRAIN_TILE_REQUEST>
	{
		static constexpr const char* shader_filename = "shaders/engine/terrain_page_request/comp.spv";
		static constexpr std::array<DSL_TYPE, 1> dsl_types =
		{
			DSL_TYPE_TERRAIN_COMPUTE
		};
		static constexpr std::array<VkPushConstantRange, 0> push_consts = {};

		struct dsl
		{
			static constexpr std::array<VkDescriptorSetLayoutBinding, 1> bindings =
			{
				0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
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