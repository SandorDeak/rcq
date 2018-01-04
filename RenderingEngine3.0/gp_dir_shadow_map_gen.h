#pragma once

#include "gp_create_info.h"
#include "vertex.h"
#include "const_dir_shadow_map_size.h"

namespace rcq
{
	template<>
	struct gp_create_info<GP_DIR_SHADOW_MAP_GEN>
	{
		static constexpr uint32_t render_pass = RP_DIR_SHADOW_MAP_GEN;
		static constexpr auto attrib = vertex::get_attribute_descriptions()[0];
		static constexpr auto binding = vertex::get_binding_description();
		static constexpr VkPipelineVertexInputStateCreateInfo vertex_input =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			nullptr,
			0,
			1,
			&binding,
			1,
			&attrib
		};
		static constexpr VkPipelineInputAssemblyStateCreateInfo input_assembly =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VK_FALSE
		};
		static constexpr VkPipelineRasterizationStateCreateInfo rasterizer =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_FALSE,
			VK_FALSE,
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_FRONT_BIT,
			VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE,
			0.f,
			0.f,
			0.f,
			1.f
		};
		static constexpr VkPipelineDepthStencilStateCreateInfo depthstencil =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_TRUE,
			VK_TRUE,
			VK_COMPARE_OP_LESS,
			VK_FALSE,
			VK_FALSE,
			{},
			{},
			0.f,
			0.f
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
		static constexpr VkViewport vp =
		{
			0.f,
			0.f,
			static_cast<float>(DIR_SHADOW_MAP_SIZE),
			static_cast<float>(DIR_SHADOW_MAP_SIZE),
			0.f,
			1.f
		};
		static constexpr VkRect2D scissor =
		{
			{ 0, 0 },
			{ DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE }
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
		static constexpr std::array<const char*, 2> shader_filenames =
		{
			"shaders/engine/dir_shadow_map_gen/vert.spv",
			"shaders/engine/dir_shadow_map_gen/geom.spv"
		};
		static constexpr std::array<VkShaderStageFlagBits, 2> shader_flags =
		{
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_SHADER_STAGE_GEOMETRY_BIT
		};
		static constexpr std::array<DSL_TYPE, 1> dsl_types =
		{
			DSL_TYPE_TR
		};

		struct dsl
		{
			static constexpr std::array<VkDescriptorSetLayoutBinding, 1> bindings =
			{
				0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				1,
				VK_SHADER_STAGE_GEOMETRY_BIT,
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

		static constexpr void fill_optional(VkGraphicsPipelineCreateInfo& c)
		{
			c.pDepthStencilState = &depthstencil;
			c.subpass = 0;
		}
	};
}