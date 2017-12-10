#pragma once

#include "gp_create_info.h"
#include <array>
#include "const_swap_chain_image_extent.h"

namespace rcq
{
	template<>
	struct gp_create_info<GP_SSR_RAY_CASTING>
	{
		static constexpr uint32_t render_pass = RP_PREIMAGE_ASSEMBLER;
		static constexpr VkPipelineVertexInputStateCreateInfo vertex_input =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			nullptr,
			0,
			0,
			nullptr,
			0,
			nullptr
		};
		static constexpr VkPipelineInputAssemblyStateCreateInfo input_assembly =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
			VK_FALSE
		};
		static constexpr VkPipelineRasterizationStateCreateInfo VkPipelineRasterizationStateCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_FALSE,
			VK_FALSE,
			VK_POLYGON_MODE_LINE,
			VK_CULL_MODE_NONE,
			VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE,
			0.f,
			0.f,
			0.f,
			1.f
		};
		static constexpr VkPipelineMultisampleStateCreateInfo multisample =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_SAMPLE_COUNT_1_BIT,
			VK_FALSE,
			0.f,
			nullptr,
			VK_FALSE,
			VK_FALSE
		};
		static constexpr VkPipelineDepthStencilStateCreateInfo depthstencil =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_TRUE,
			VK_FALSE,
			VK_COMPARE_OP_GREATER,
			VK_FALSE,
			VK_FALSE,
			{},
			{},
			0.f,
			0.f
		};
		static constexpr VkViewport vp =
		{
			0.f,
			0.f,
			static_cast<float>(SWAP_CHAIN_IMAGE_EXTENT.width),
			static_cast<float>(SWAP_CHAIN_IMAGE_EXTENT.height),
			0.f,
			1.f
		};
		static constexpr VkRect2D scissor =
		{
			{ 0, 0 },
			SWAP_CHAIN_IMAGE_EXTENT
		};
		static constexpr VkPipelineViewportStateCreateInfo viewport =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			nullptr,
			0,
			1,
			&vp,
			1,
			&scissor
		};
		static constexpr std::array<const char*, 3> shader_filenames =
		{
			"shaders/gta5_pass/ssr_ray_casting/vert.spv",
			"shaders/gta5_pass/ssr_ray_casting/geom.spv",
			"shaders/gta5_pass/ssr_ray_casting/frag.spv"
		};
		static constexpr std::array<VkShaderStageFlagBits, 3> shader_flags =
		{
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_SHADER_STAGE_GEOMETRY_BIT,
			VK_SHADER_STAGE_FRAGMENT_BIT
		};
		static constexpr std::array<DSL_TYPE, 0> dsl_types = {};

		struct dsl
		{
			static constexpr std::array<VkDescriptorSetLayoutBinding, 4> bindings = 
			{
				0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				1,
				VK_SHADER_STAGE_GEOMETRY_BIT,
				nullptr,

				0,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				VK_SHADER_STAGE_GEOMETRY_BIT,
				nullptr,

				0,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				VK_SHADER_STAGE_GEOMETRY_BIT,
				nullptr,

				0,
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				1,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				nullptr
			};
			static constexpr VkDescriptorSetLayoutCreateInfo create_info =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				nullptr,
				0,
				bindings.size(),
				bindings.data()
			};
		};

		static constexpr void fill_optional(VkGraphicsPipelineCreateInfo& c)
		{
			c.pDepthStencilState = &depthstencil;
			c.subpass = 2;
		}
	};
}