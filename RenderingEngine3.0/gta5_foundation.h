#pragma once

#include "foundation.h"

namespace rcq
{
	const uint32_t ENVIRONMENT_MAP_SIZE = 128;
	const uint32_t DIR_SHADOW_MAP_SIZE = 1024;
	const uint32_t FRUSTUM_SPLIT_COUNT = 2;

	enum GP
	{
		GP_ENVIRONMENT_MAP_GEN_MAT,
		GP_ENVIRONMENT_MAP_GEN_SKYBOX,

		GP_DIR_SHADOW_MAP_GEN,

		GP_GBUFFER_GEN,
		GP_SS_DIR_SHADOW_MAP_GEN,
		GP_SS_DIR_SHADOW_MAP_BLUR,
		GP_SSAO_GEN,
		GP_SSAO_BLUR,
		GP_IMAGE_ASSEMBLER,

		GP_POSTPROCESSING,
		GP_COUNT
	};

	namespace render_pass_environment_map_gen
	{
		enum ATT
		{
			ATT_DEPTH,
			ATT_COLOR,
			ATT_COUNT
		};

		enum DEP
		{
			DEP_BEGIN,
			DEP_END,
			DEP_COUNT
		};

		namespace subpass_unique
		{
			constexpr VkAttachmentReference ref_color = { ATT_COLOR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			constexpr VkAttachmentReference ref_depth = { ATT_DEPTH, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription subpass = {};
				subpass.colorAttachmentCount = 1;
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.pColorAttachments = &ref_color;
				subpass.pDepthStencilAttachment = &ref_depth;
				return subpass;
			}

			namespace pipelines
			{			
				namespace mat
				{
					constexpr auto attribs = vertex::get_attribute_descriptions();
					constexpr auto binding = vertex::get_binding_description();

					constexpr auto create_vertex_input()
					{
						VkPipelineVertexInputStateCreateInfo input = {};
						input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
						input.pVertexAttributeDescriptions = attribs.data();
						input.pVertexBindingDescriptions = &binding;
						input.vertexAttributeDescriptionCount = attribs.size();
						input.vertexBindingDescriptionCount = 1;
						return input;
					}
					constexpr auto vertex_input = create_vertex_input();

					constexpr auto create_input_assembly()
					{

						VkPipelineInputAssemblyStateCreateInfo assembly = {};
						assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
						assembly.primitiveRestartEnable = VK_FALSE;
						assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
						return assembly;
					}
					constexpr auto input_assembly = create_input_assembly();

					constexpr auto create_rasterizer()
					{
						VkPipelineRasterizationStateCreateInfo r = {};
						r.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
						r.cullMode = VK_CULL_MODE_BACK_BIT;
						r.depthBiasEnable = VK_FALSE;
						r.depthClampEnable = VK_FALSE;
						r.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
						r.lineWidth = 1.f;
						r.polygonMode = VK_POLYGON_MODE_FILL;
						r.rasterizerDiscardEnable = VK_FALSE;
						return r;
					}
					constexpr auto rasterizer = create_rasterizer();

					constexpr auto create_depthstencil()
					{
						VkPipelineDepthStencilStateCreateInfo d = {};
						d.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
						d.depthBoundsTestEnable = VK_FALSE;
						d.depthTestEnable = VK_TRUE;
						d.depthWriteEnable = VK_TRUE;
						d.depthCompareOp = VK_COMPARE_OP_LESS;
						d.stencilTestEnable = VK_FALSE;
						return d;
					}
					constexpr auto depthstencil = create_depthstencil();

					constexpr auto create_multisample()
					{
						VkPipelineMultisampleStateCreateInfo m = {};
						m.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
						m.alphaToCoverageEnable = VK_FALSE;
						m.alphaToOneEnable = VK_FALSE;
						m.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
						m.sampleShadingEnable = VK_FALSE;
						return m;
					}
					constexpr auto multisample = create_multisample();

					constexpr auto create_blend_att()
					{
						VkPipelineColorBlendAttachmentState b = {};
						b.blendEnable = VK_FALSE;
						b.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
							| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
						return b;
					}
					constexpr auto blend_att = create_blend_att();

					constexpr auto create_blend()
					{
						VkPipelineColorBlendStateCreateInfo b = {};
						b.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
						b.attachmentCount = 1;
						b.pAttachments = &blend_att;
						b.logicOpEnable = VK_FALSE;
						return b;
					}
					constexpr auto blend = create_blend();

					constexpr auto create_vp()
					{
						VkViewport v = {};
						v.height = static_cast<float>(ENVIRONMENT_MAP_SIZE);
						v.width = static_cast<float>(ENVIRONMENT_MAP_SIZE);
						v.maxDepth = 1.f;
						v.minDepth = 0.f;
						v.x = 0.f;
						v.y = 0.f;
						return v;
						
					}
					constexpr VkViewport vp = create_vp();

					constexpr auto create_scissor()
					{
						VkRect2D s = {};
						s.extent.height = ENVIRONMENT_MAP_SIZE;
						s.extent.width = ENVIRONMENT_MAP_SIZE;
						s.offset = { 0,0 };
						return s;
					}
					constexpr VkRect2D scissor=create_scissor();

					constexpr auto create_viewport()
					{
						VkPipelineViewportStateCreateInfo v = {};
						v.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
						v.pScissors = &scissor;
						v.scissorCount = 1;
						v.pViewports = &vp;
						v.viewportCount = 1;
						return v;
					}
					constexpr auto viewport = create_viewport();

					struct runtime_info
					{
						runtime_info(VkDevice d) : device(d)
						{
							
							create_shaders(device,
							{
								"shaders/gta5_pass/environment_map_gen/environment_map_gen_mat/vert.spv",
								"shaders/gta5_pass/environment_map_gen/environment_map_gen_mat/geom.spv",
								"shaders/gta5_pass/environment_map_gen/environment_map_gen_mat/frag.spv"
							},
							{
								VK_SHADER_STAGE_VERTEX_BIT,
								VK_SHADER_STAGE_GEOMETRY_BIT,
								VK_SHADER_STAGE_FRAGMENT_BIT
							},
								shaders.data()
							);						
						}

						~runtime_info()
						{
							for (auto s : shaders)
								vkDestroyShaderModule(device, s.module, host_memory_manager);
						}

						void fill_create_info(VkGraphicsPipelineCreateInfo& c)
						{
							c.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
							c.basePipelineHandle = VK_NULL_HANDLE;
							c.basePipelineIndex = -1;
							c.pColorBlendState = &blend;
							c.pDepthStencilState = &depthstencil;
							c.pInputAssemblyState = &input_assembly;
							c.pMultisampleState = &multisample;
							c.pRasterizationState = &rasterizer;
							c.pStages = shaders.data();
							c.stageCount = 3;
							c.pVertexInputState = &vertex_input;
							c.pViewportState = &viewport;
							c.subpass = 0;
						}

						VkDevice device;
						std::array<VkPipelineShaderStageCreateInfo, 3> shaders = {};
					};

					namespace dsl
					{
						constexpr auto create_binding()
						{
							VkDescriptorSetLayoutBinding binding = {};
							binding.binding = 0;
							binding.descriptorCount = 1;
							binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
							binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
							return binding;
						}
						constexpr auto binding = create_binding();

						constexpr auto create_create_info()
						{
							VkDescriptorSetLayoutCreateInfo dsl = {};
							dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
							dsl.bindingCount = 1;
							dsl.pBindings = &binding;
							return dsl;
						}
						constexpr auto create_info = create_create_info();
					}//namespace dsl
				}//namespace mat

				namespace skybox
				{
					constexpr auto create_vertex_input()
					{
						VkPipelineVertexInputStateCreateInfo input = {};
						input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
						return input;
					}
					constexpr auto vertex_input = create_vertex_input();

					constexpr auto create_input_assembly()
					{

						VkPipelineInputAssemblyStateCreateInfo assembly = {};
						assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
						assembly.primitiveRestartEnable = VK_FALSE;
						assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
						return assembly;
					}
					constexpr auto input_assembly = create_input_assembly();

					constexpr auto create_rasterizer()
					{
						VkPipelineRasterizationStateCreateInfo r = {};
						r.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
						r.cullMode = VK_CULL_MODE_NONE;
						r.depthBiasEnable = VK_FALSE;
						r.depthClampEnable = VK_FALSE;
						r.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
						r.lineWidth = 1.f;
						r.polygonMode = VK_POLYGON_MODE_FILL;
						r.rasterizerDiscardEnable = VK_FALSE;
						return r;
					}
					constexpr auto rasterizer = create_rasterizer();

					constexpr auto create_depthstencil()
					{
						VkPipelineDepthStencilStateCreateInfo d = {};
						d.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
						d.depthBoundsTestEnable = VK_FALSE;
						d.depthTestEnable = VK_TRUE;
						d.depthWriteEnable = VK_FALSE;
						d.depthCompareOp = VK_COMPARE_OP_EQUAL;
						d.stencilTestEnable = VK_FALSE;
						return d;
					}
					constexpr auto depthstencil = create_depthstencil();

					constexpr auto create_multisample()
					{
						VkPipelineMultisampleStateCreateInfo m = {};
						m.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
						m.alphaToCoverageEnable = VK_FALSE;
						m.alphaToOneEnable = VK_FALSE;
						m.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
						m.sampleShadingEnable = VK_FALSE;
						return m;
					}
					constexpr auto multisample = create_multisample();

					constexpr auto create_blend_att()
					{
						VkPipelineColorBlendAttachmentState b = {};
						b.blendEnable = VK_FALSE;
						b.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
							| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
						return b;
					}
					constexpr auto blend_att = create_blend_att();

					constexpr auto create_blend()
					{
						VkPipelineColorBlendStateCreateInfo b = {};
						b.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
						b.attachmentCount = 1;
						b.pAttachments = &blend_att;
						b.logicOpEnable = VK_FALSE;
						return b;
					}
					constexpr auto blend = create_blend();

					constexpr auto create_vp()
					{
						VkViewport v = {};
						v.height = static_cast<float>(ENVIRONMENT_MAP_SIZE);
						v.width = static_cast<float>(ENVIRONMENT_MAP_SIZE);
						v.maxDepth = 1.f;
						v.minDepth = 0.f;
						v.x = 0.f;
						v.y = 0.f;
						return v;

					}
					constexpr VkViewport vp = create_vp();

					constexpr auto create_scissor()
					{
						VkRect2D s = {};
						s.extent.height = ENVIRONMENT_MAP_SIZE;
						s.extent.width = ENVIRONMENT_MAP_SIZE;
						s.offset = { 0,0 };
						return s;
					}
					constexpr VkRect2D scissor = create_scissor();

					constexpr auto create_viewport()
					{
						VkPipelineViewportStateCreateInfo v = {};
						v.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
						v.pScissors = &scissor;
						v.scissorCount = 1;
						v.pViewports = &vp;
						v.viewportCount = 1;
						return v;
					}
					constexpr auto viewport = create_viewport();

					struct runtime_info
					{
						runtime_info(VkDevice d) : device(d)
						{

							create_shaders(device,
							{
								"shaders/gta5_pass/environment_map_gen/environment_map_gen_skybox/vert.spv",
								"shaders/gta5_pass/environment_map_gen/environment_map_gen_skybox/geom.spv",
								"shaders/gta5_pass/environment_map_gen/environment_map_gen_skybox/frag.spv"
							},
							{
								VK_SHADER_STAGE_VERTEX_BIT,
								VK_SHADER_STAGE_GEOMETRY_BIT,
								VK_SHADER_STAGE_FRAGMENT_BIT
							},
								shaders.data()
							);
						}

						~runtime_info()
						{
							for (auto s : shaders)
								vkDestroyShaderModule(device, s.module, host_memory_manager);
						}

						void fill_create_info(VkGraphicsPipelineCreateInfo& c)
						{
							c.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
							c.basePipelineHandle = VK_NULL_HANDLE;
							c.basePipelineIndex = -1;
							c.pColorBlendState = &blend;
							c.pDepthStencilState = &depthstencil;
							c.pInputAssemblyState = &input_assembly;
							c.pMultisampleState = &multisample;
							c.pRasterizationState = &rasterizer;
							c.pStages = shaders.data();
							c.stageCount = 3;
							c.pVertexInputState = &vertex_input;
							c.pViewportState = &viewport;
							c.subpass = 0;
						}

						VkDevice device;
						std::array<VkPipelineShaderStageCreateInfo, 3> shaders = {};
					};

					namespace dsl
					{
						constexpr auto create_binding()
						{
							VkDescriptorSetLayoutBinding binding = {};
							binding.binding = 0;
							binding.descriptorCount = 1;
							binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
							binding.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
							return binding;
						}
						constexpr auto binding = create_binding();

						constexpr auto create_create_info()
						{
							VkDescriptorSetLayoutCreateInfo dsl = {};
							dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
							dsl.bindingCount = 1;
							dsl.pBindings = &binding;
							return dsl;
						}
						constexpr auto create_info = create_create_info();
					}//namespace dsl
				}//namespace skybox
			}//namespace pipeline
		}//namespace subpass_unique

		constexpr std::array<VkAttachmentDescription, ATT_COUNT> get_attachments()
		{

			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};
			atts[ATT_DEPTH].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_DEPTH].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			atts[ATT_DEPTH].format = VK_FORMAT_D32_SFLOAT;
			atts[ATT_DEPTH].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_DEPTH].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			atts[ATT_DEPTH].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_DEPTH].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_DEPTH].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			atts[ATT_COLOR].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_COLOR].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			atts[ATT_COLOR].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_COLOR].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_COLOR].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_COLOR].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_COLOR].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_COLOR].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			return atts;
		}

		constexpr auto get_dependencies()
		{
			std::array<VkSubpassDependency, DEP_COUNT> d = {};
			d[DEP_BEGIN].srcSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_BEGIN].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_BEGIN].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			d[DEP_BEGIN].dstSubpass = 0;
			d[DEP_BEGIN].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_BEGIN].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			d[DEP_END].srcSubpass = 0;
			d[DEP_END].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_END].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_END].dstSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_END].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_END].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			return d;
		}

		constexpr std::array<VkSubpassDependency, DEP_COUNT> deps=get_dependencies();
		constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts=get_attachments();

		constexpr VkSubpassDescription sp_unique=subpass_unique::create_subpass();

		constexpr VkRenderPassCreateInfo create_create_info()
		{
			VkRenderPassCreateInfo pass = {};
			pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			pass.attachmentCount = ATT_COUNT;
			pass.dependencyCount = DEP_COUNT;
			pass.pAttachments = atts.data();
			pass.subpassCount = 1;
			pass.pSubpasses = &sp_unique;
			pass.pDependencies = deps.data();
			return pass;
		}

		constexpr VkRenderPassCreateInfo create_info=create_create_info();
	}//namespace render_pass_environment_map_gen


	namespace render_pass_dir_shadow_map_gen
	{
		enum ATT
		{
			ATT_DEPTH,
			ATT_COUNT
		};

		enum DEP
		{
			DEP_BEGIN,
			DEP_END,
			DEP_COUNT
		};

		namespace subpass_unique
		{
			constexpr VkAttachmentReference ref_depth = { ATT_DEPTH,  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.pDepthStencilAttachment = &ref_depth;
				subpass.colorAttachmentCount = 0;
				return subpass;
			}

			namespace pipeline
			{
				constexpr auto attrib = vertex::get_attribute_descriptions()[0];
				constexpr auto binding = vertex::get_binding_description();

				constexpr auto create_vertex_input()
				{
					VkPipelineVertexInputStateCreateInfo input = {};
					input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
					input.pVertexAttributeDescriptions = &attrib;
					input.pVertexBindingDescriptions = &binding;
					input.vertexAttributeDescriptionCount = 1;
					input.vertexBindingDescriptionCount = 1;
					return input;
				}
				constexpr auto vertex_input = create_vertex_input();

				constexpr auto create_input_assembly()
				{

					VkPipelineInputAssemblyStateCreateInfo assembly = {};
					assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
					assembly.primitiveRestartEnable = VK_FALSE;
					assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					return assembly;
				}
				constexpr auto input_assembly = create_input_assembly();

				constexpr auto create_rasterizer()
				{
					VkPipelineRasterizationStateCreateInfo r = {};
					r.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
					r.cullMode = VK_CULL_MODE_BACK_BIT;
					r.depthBiasEnable = VK_FALSE;
					r.depthClampEnable = VK_FALSE;
					r.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
					r.lineWidth = 1.f;
					r.polygonMode = VK_POLYGON_MODE_FILL;
					r.rasterizerDiscardEnable = VK_FALSE;
					return r;
				}
				constexpr auto rasterizer = create_rasterizer();

				constexpr auto create_depthstencil()
				{
					VkPipelineDepthStencilStateCreateInfo d = {};
					d.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
					d.depthBoundsTestEnable = VK_FALSE;
					d.depthTestEnable = VK_TRUE;
					d.depthWriteEnable = VK_TRUE;
					d.depthCompareOp = VK_COMPARE_OP_LESS;
					d.stencilTestEnable = VK_FALSE;
					return d;
				}
				constexpr auto depthstencil = create_depthstencil();

				constexpr auto create_multisample()
				{
					VkPipelineMultisampleStateCreateInfo m = {};
					m.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
					m.alphaToCoverageEnable = VK_FALSE;
					m.alphaToOneEnable = VK_FALSE;
					m.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
					m.sampleShadingEnable = VK_FALSE;
					return m;
				}
				constexpr auto multisample = create_multisample();

				constexpr auto create_vp()
				{
					VkViewport v = {};
					v.height = static_cast<float>(DIR_SHADOW_MAP_SIZE);
					v.width = static_cast<float>(DIR_SHADOW_MAP_SIZE);
					v.maxDepth = 1.f;
					v.minDepth = 0.f;
					v.x = 0.f;
					v.y = 0.f;
					return v;

				}
				constexpr VkViewport vp = create_vp();

				constexpr auto create_scissor()
				{
					VkRect2D s = {};
					s.extent.height = DIR_SHADOW_MAP_SIZE;
					s.extent.width = DIR_SHADOW_MAP_SIZE;
					s.offset = { 0,0 };
					return s;
				}
				constexpr VkRect2D scissor = create_scissor();

				constexpr auto create_viewport()
				{
					VkPipelineViewportStateCreateInfo v = {};
					v.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
					v.pScissors = &scissor;
					v.scissorCount = 1;
					v.pViewports = &vp;
					v.viewportCount = 1;
					return v;
				}
				constexpr auto viewport = create_viewport();

				struct runtime_info
				{
					runtime_info(VkDevice d) : device(d)
					{

						create_shaders(device,
						{
							"shaders/gta5_pass/dir_shadow_map_gen/vert.spv",
							"shaders/gta5_pass/dir_shadow_map_gen/geom.spv"
						},
						{
							VK_SHADER_STAGE_VERTEX_BIT,
							VK_SHADER_STAGE_GEOMETRY_BIT
						},
							shaders.data()
						);
					}

					~runtime_info()
					{
						for (auto s : shaders)
							vkDestroyShaderModule(device, s.module, host_memory_manager);
					}

					void fill_create_info(VkGraphicsPipelineCreateInfo& c)
					{
						c.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
						c.basePipelineHandle = VK_NULL_HANDLE;
						c.basePipelineIndex = -1;
						c.pDepthStencilState = &depthstencil;
						c.pInputAssemblyState = &input_assembly;
						c.pMultisampleState = &multisample;
						c.pRasterizationState = &rasterizer;
						c.pStages = shaders.data();
						c.stageCount = 2;
						c.pVertexInputState = &vertex_input;
						c.pViewportState = &viewport;
						c.subpass = 0;
					}

					VkDevice device;
					std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {};
				};

				namespace dsl
				{
					constexpr auto create_binding()
					{
						VkDescriptorSetLayoutBinding binding = {};
						binding.binding = 0;
						binding.descriptorCount = 1;
						binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						binding.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
						return binding;
					}
					constexpr auto binding = create_binding();

					constexpr auto create_create_info()
					{
						VkDescriptorSetLayoutCreateInfo dsl = {};
						dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
						dsl.bindingCount = 1;
						dsl.pBindings = &binding;
						return dsl;
					}
					constexpr auto create_info = create_create_info();
				}//namespace dsl
				
			}//namespace pipeline
		}//namespace subpass_unique

		constexpr std::array<VkAttachmentDescription, ATT_COUNT> get_attachments()
		{

			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};
			atts[ATT_DEPTH].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_DEPTH].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			atts[ATT_DEPTH].format = VK_FORMAT_D32_SFLOAT;
			atts[ATT_DEPTH].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_DEPTH].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			atts[ATT_DEPTH].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_DEPTH].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_DEPTH].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			return atts;
		}

		constexpr auto get_dependencies()
		{
			std::array<VkSubpassDependency, DEP_COUNT> d = {};
			d[DEP_BEGIN].srcSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_BEGIN].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_BEGIN].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			d[DEP_BEGIN].dstSubpass = 0;
			d[DEP_BEGIN].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			d[DEP_BEGIN].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

			d[DEP_END].srcSubpass = 0;
			d[DEP_END].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			d[DEP_END].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			d[DEP_END].dstSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_END].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_END].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			return d;
		}

		constexpr std::array<VkSubpassDependency, DEP_COUNT> deps=get_dependencies();
		std::array<VkAttachmentDescription, ATT_COUNT> atts=get_attachments();
		constexpr VkSubpassDescription sp_unique=subpass_unique::create_subpass();
		
		constexpr VkRenderPassCreateInfo create_create_info()
		{
			VkRenderPassCreateInfo pass = {};
			pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			pass.attachmentCount = ATT_COUNT;
			pass.dependencyCount = DEP_COUNT;
			pass.pAttachments = atts.data();
			pass.subpassCount = 1;
			pass.pSubpasses = &sp_unique;
			pass.pDependencies = deps.data();
			return pass;
		}
		constexpr VkRenderPassCreateInfo create_info=create_create_info();
	}//namespace render_pass_dir_shadow_map_gen



	namespace render_pass_frame_image_gen
	{
		enum ATT
		{
			ATT_DEPTHSTENCIL,
			ATT_POS_ROUGHNESS,
			ATT_F0_SSAO,
			ATT_ALBEDO_SSDS,
			ATT_NORMAL_AO,
			ATT_SS_DIR_SHADOW_MAP,
			ATT_SSAO_MAP,
			ATT_PREIMAGE,
			ATT_COUNT
		};

		enum SUBPASS
		{
			SUBPASS_GBUFFER_GEN,
			SUBPASS_SS_DIR_SHADOW_MAP_GEN,
			SUBPASS_SS_DIR_SHADOW_MAP_BLUR,
			SUBPASS_SSAO_MAP_GEN,
			SUBPASS_SSAO_MAP_BLUR,
			SUBPASS_IMAGE_ASSEMBLER,
			SUBPASS_COUNT
		};

		enum DEP
		{
			//DEP_EXT_SSDS_GEN,
			DEP_GBUFFER_GEN_SSDS_GEN,
			DEP_GBUFFER_GEN_SSAO_GEN,
			DEP_GBUFFER_GEN_SSDS_BLUR,
			DEP_GBUFFER_GEN_SSAO_BLUR,
			DEP_SSDS_GEN_SELF,
			DEP_SSAO_GEN_SELF,

			DEP_SSDS_BLUR_IMAGE_ASSEMBLER,
			DEP_SSAO_BLUR_IMAGE_ASSEMBLER,
			DEP_GBUFFER_GEN_IMAGE_ASSEMBLER,
			//DEP_END,
			DEP_COUNT
		};

		namespace subpass_gbuffer_gen
		{
			enum REF
			{
				REF_POS_ROUGHNESS,
				REF_FO_SSAO,
				REF_ALBEDO_SSDS,
				REF_NORMAL_AO,
				REF_COUNT
			};

			constexpr auto get_refs_color()
			{
				std::array<VkAttachmentReference, REF_COUNT> refs = {};
				refs[REF_POS_ROUGHNESS].attachment = ATT_POS_ROUGHNESS;
				refs[REF_POS_ROUGHNESS].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				refs[REF_FO_SSAO].attachment = ATT_F0_SSAO;
				refs[REF_FO_SSAO].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				refs[REF_ALBEDO_SSDS].attachment = ATT_ALBEDO_SSDS;
				refs[REF_ALBEDO_SSDS].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				refs[REF_NORMAL_AO].attachment = ATT_NORMAL_AO;
				refs[REF_NORMAL_AO].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				return refs;
			}

			constexpr std::array<VkAttachmentReference, REF_COUNT> refs_color=get_refs_color();
			constexpr VkAttachmentReference ref_depth = { ATT_DEPTHSTENCIL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = REF_COUNT;
				s.pColorAttachments = refs_color.data();
				s.pDepthStencilAttachment = &ref_depth;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			namespace pipeline
			{
				constexpr auto attribs = get_vertex_input_attribute_descriptions();
				constexpr auto bindings = get_vertex_input_binding_descriptions();

				constexpr auto create_vertex_input()
				{
					VkPipelineVertexInputStateCreateInfo input = {};
					input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
					input.pVertexAttributeDescriptions = attribs.data();
					input.pVertexBindingDescriptions = bindings.data();
					input.vertexAttributeDescriptionCount = attribs.size();
					input.vertexBindingDescriptionCount = bindings.size();
					return input;
				}
				constexpr auto vertex_input = create_vertex_input();

				constexpr auto create_input_assembly()
				{

					VkPipelineInputAssemblyStateCreateInfo assembly = {};
					assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
					assembly.primitiveRestartEnable = VK_FALSE;
					assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					return assembly;
				}
				constexpr auto input_assembly = create_input_assembly();

				constexpr auto create_rasterizer()
				{
					VkPipelineRasterizationStateCreateInfo r = {};
					r.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
					r.cullMode = VK_CULL_MODE_BACK_BIT;
					r.depthBiasEnable = VK_FALSE;
					r.depthClampEnable = VK_FALSE;
					r.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
					r.lineWidth = 1.f;
					r.polygonMode = VK_POLYGON_MODE_FILL;
					r.rasterizerDiscardEnable = VK_FALSE;
					return r;
				}
				constexpr auto rasterizer = create_rasterizer();

				constexpr auto create_depthstencil()
				{
					VkPipelineDepthStencilStateCreateInfo d = {};
					d.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
					d.depthBoundsTestEnable = VK_FALSE;
					d.depthTestEnable = VK_TRUE;
					d.depthWriteEnable = VK_TRUE;
					d.depthCompareOp = VK_COMPARE_OP_LESS;
					d.stencilTestEnable = VK_FALSE;
					return d;
				}
				constexpr auto depthstencil = create_depthstencil();

				constexpr auto create_multisample()
				{
					VkPipelineMultisampleStateCreateInfo m = {};
					m.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
					m.alphaToCoverageEnable = VK_FALSE;
					m.alphaToOneEnable = VK_FALSE;
					m.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
					m.sampleShadingEnable = VK_FALSE;
					return m;
				}
				constexpr auto multisample = create_multisample();

				constexpr auto create_blend_atts()
				{
					std::array<VkPipelineColorBlendAttachmentState, 4> bs = {};
					for (auto& b : bs)
					{
						b.blendEnable = VK_FALSE;
						b.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
							| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
					}
					return bs;
				}
				constexpr auto blend_atts = create_blend_atts();

				constexpr auto create_blend()
				{
					VkPipelineColorBlendStateCreateInfo b = {};
					b.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
					b.attachmentCount = blend_atts.size();
					b.pAttachments = blend_atts.data();
					b.logicOpEnable = VK_FALSE;
					return b;
				}
				constexpr auto blend = create_blend();

				struct runtime_info
				{
					runtime_info(VkDevice d, const VkExtent2D& e) : device(d)
					{

						create_shaders(device,
						{
							"shaders/gta5_pass/gbuffer_gen/vert.spv",
							"shaders/gta5_pass/gbuffer_gen/frag.spv"
						},
						{
							VK_SHADER_STAGE_VERTEX_BIT,
							VK_SHADER_STAGE_FRAGMENT_BIT
						},
							shaders.data()
						);

						viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
						viewport.pScissors = &scissor;
						viewport.scissorCount = 1;
						viewport.pViewports = &vp;
						viewport.viewportCount = 1;
						scissor.extent = e;
						scissor.offset = { 0,0 };
						vp.height = static_cast<float>(e.height);
						vp.width = static_cast<float>(e.width);
						vp.maxDepth = 1.f;
						vp.minDepth = 0.f;
						vp.x = 0.f;
						vp.y = 0.f;
					}

					~runtime_info()
					{
						for (auto s : shaders)
							vkDestroyShaderModule(device, s.module, host_memory_manager);
					}

					void fill_create_info(VkGraphicsPipelineCreateInfo& c)
					{
						c.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
						c.basePipelineHandle = VK_NULL_HANDLE;
						c.basePipelineIndex = -1;
						c.pDepthStencilState = &depthstencil;
						c.pInputAssemblyState = &input_assembly;
						c.pColorBlendState = &blend;
						c.pMultisampleState = &multisample;
						c.pRasterizationState = &rasterizer;
						c.pStages = shaders.data();
						c.stageCount = 2;
						c.pVertexInputState = &vertex_input;
						c.pViewportState = &viewport;
						c.subpass = SUBPASS_GBUFFER_GEN;
					}

					VkDevice device;
					std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {};
					VkViewport vp;
					VkRect2D scissor;
					VkPipelineViewportStateCreateInfo viewport = {};
				};
				namespace dsl
				{
					constexpr auto create_binding()
					{
						VkDescriptorSetLayoutBinding binding = {};
						binding.binding = 0;
						binding.descriptorCount = 1;
						binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
						return binding;
					}
					constexpr auto binding = create_binding();

					constexpr auto create_create_info()
					{
						VkDescriptorSetLayoutCreateInfo dsl = {};
						dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
						dsl.bindingCount = 1;
						dsl.pBindings = &binding;
						return dsl;
					}
					constexpr auto create_info = create_create_info();
				}//namespace dsl
			}//namespace pipeline
		}//namespace subpass_gbuffer_gen

		namespace subpass_ss_dir_shadow_map_gen
		{
			constexpr std::array<uint32_t, 3> pres = { ATT_F0_SSAO, ATT_ALBEDO_SSDS, ATT_NORMAL_AO };
			constexpr VkAttachmentReference depth = { ATT_SS_DIR_SHADOW_MAP,  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
			constexpr VkAttachmentReference pos_in = { ATT_POS_ROUGHNESS, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.pDepthStencilAttachment = &depth;
				s.pInputAttachments = &pos_in;
				s.inputAttachmentCount = 1;
				s.preserveAttachmentCount = 3;
				s.pPreserveAttachments = pres.data();
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			namespace pipeline
			{

				constexpr auto create_vertex_input()
				{
					VkPipelineVertexInputStateCreateInfo input = {};
					input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
					return input;
				}
				constexpr auto vertex_input = create_vertex_input();

				constexpr auto create_input_assembly()
				{

					VkPipelineInputAssemblyStateCreateInfo assembly = {};
					assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
					assembly.primitiveRestartEnable = VK_FALSE;
					assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
					return assembly;
				}
				constexpr auto input_assembly = create_input_assembly();

				constexpr auto create_rasterizer()
				{
					VkPipelineRasterizationStateCreateInfo r = {};
					r.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
					r.cullMode = VK_CULL_MODE_NONE;
					r.depthBiasEnable = VK_FALSE;
					r.depthClampEnable = VK_FALSE;
					r.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
					r.lineWidth = 1.f;
					r.polygonMode = VK_POLYGON_MODE_FILL;
					r.rasterizerDiscardEnable = VK_FALSE;
					return r;
				}
				constexpr auto rasterizer = create_rasterizer();

				constexpr auto create_depth_stencil()
				{
					VkPipelineDepthStencilStateCreateInfo d = {};
					d.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
					d.depthBoundsTestEnable = VK_FALSE;
					d.depthCompareOp = VK_COMPARE_OP_ALWAYS;
					d.depthTestEnable = VK_TRUE;
					d.depthWriteEnable = VK_TRUE;
					d.stencilTestEnable = VK_FALSE;
					return d;
				}
				constexpr auto depth = create_depth_stencil();

				constexpr auto create_multisample()
				{
					VkPipelineMultisampleStateCreateInfo m = {};
					m.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
					m.alphaToCoverageEnable = VK_FALSE;
					m.alphaToOneEnable = VK_FALSE;
					m.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
					m.sampleShadingEnable = VK_FALSE;
					return m;
				}
				constexpr auto multisample = create_multisample();

				struct runtime_info
				{
					runtime_info(VkDevice d, const VkExtent2D& e) : device(d)
					{

						create_shaders(device,
						{
							"shaders/gta5_pass/ss_dir_shadow_map_gen/vert.spv",
							"shaders/gta5_pass/ss_dir_shadow_map_gen/frag.spv"
						},
						{
							VK_SHADER_STAGE_VERTEX_BIT,
							VK_SHADER_STAGE_FRAGMENT_BIT
						},
							shaders.data()
						);

						viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
						viewport.pScissors = &scissor;
						viewport.scissorCount = 1;
						viewport.pViewports = &vp;
						viewport.viewportCount = 1;
						scissor.extent = e;
						scissor.offset = { 0,0 };
						vp.height = static_cast<float>(e.height);
						vp.width = static_cast<float>(e.width);
						vp.maxDepth = 1.f;
						vp.minDepth = 0.f;
						vp.x = 0.f;
						vp.y = 0.f;
					}

					~runtime_info()
					{
						for (auto s : shaders)
							vkDestroyShaderModule(device, s.module, host_memory_manager);
					}

					void fill_create_info(VkGraphicsPipelineCreateInfo& c)
					{
						c.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
						c.basePipelineHandle = VK_NULL_HANDLE;
						c.basePipelineIndex = -1;
						c.pInputAssemblyState = &input_assembly;
						c.pMultisampleState = &multisample;
						c.pRasterizationState = &rasterizer;
						c.pDepthStencilState = &depth;
						c.pStages = shaders.data();
						c.stageCount = 2;
						c.pVertexInputState = &vertex_input;
						c.pViewportState = &viewport;
						c.subpass = SUBPASS_SS_DIR_SHADOW_MAP_GEN;
					}

					VkDevice device;
					std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {};
					VkViewport vp;
					VkRect2D scissor;
					VkPipelineViewportStateCreateInfo viewport = {};
				};
				namespace dsl
				{
					constexpr auto create_bindings()
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

						return bindings;
					}
					constexpr auto bindings = create_bindings();

					constexpr auto create_create_info()
					{
						VkDescriptorSetLayoutCreateInfo dsl = {};
						dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
						dsl.bindingCount = bindings.size();
						dsl.pBindings = bindings.data();
						return dsl;
					}
					constexpr auto create_info = create_create_info();
				}//namespace dsl
			}//namespace pipeline
		}//namespace subpass_ss_dir_shadow_map_gen

		namespace subpass_ss_dir_shadow_map_blur
		{
			constexpr VkAttachmentReference ssds_out = { ATT_ALBEDO_SSDS, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			constexpr VkAttachmentReference normal_in = { ATT_NORMAL_AO, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

			constexpr std::array<uint32_t, 2> pres = { ATT_F0_SSAO, ATT_POS_ROUGHNESS };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &ssds_out;
				s.inputAttachmentCount = 1;
				s.pInputAttachments = &normal_in;
				s.preserveAttachmentCount = 2;
				s.pPreserveAttachments = pres.data();
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			namespace pipeline
			{

				constexpr auto create_vertex_input()
				{
					VkPipelineVertexInputStateCreateInfo input = {};
					input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
					return input;
				}
				constexpr auto vertex_input = create_vertex_input();

				constexpr auto create_input_assembly()
				{

					VkPipelineInputAssemblyStateCreateInfo assembly = {};
					assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
					assembly.primitiveRestartEnable = VK_FALSE;
					assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
					return assembly;
				}
				constexpr auto input_assembly = create_input_assembly();

				constexpr auto create_rasterizer()
				{
					VkPipelineRasterizationStateCreateInfo r = {};
					r.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
					r.cullMode = VK_CULL_MODE_NONE;
					r.depthBiasEnable = VK_FALSE;
					r.depthClampEnable = VK_FALSE;
					r.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
					r.lineWidth = 1.f;
					r.polygonMode = VK_POLYGON_MODE_FILL;
					r.rasterizerDiscardEnable = VK_FALSE;
					return r;
				}
				constexpr auto rasterizer = create_rasterizer();

				constexpr auto create_multisample()
				{
					VkPipelineMultisampleStateCreateInfo m = {};
					m.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
					m.alphaToCoverageEnable = VK_FALSE;
					m.alphaToOneEnable = VK_FALSE;
					m.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
					m.sampleShadingEnable = VK_FALSE;
					return m;
				}
				constexpr auto multisample = create_multisample();

				constexpr auto create_blend_att()
				{
					VkPipelineColorBlendAttachmentState b = {};

					b.blendEnable = VK_FALSE;
					b.colorWriteMask = VK_COLOR_COMPONENT_A_BIT;
					return b;
				}
				constexpr auto blend_att = create_blend_att();

				constexpr auto create_blend()
				{
					VkPipelineColorBlendStateCreateInfo b = {};
					b.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
					b.attachmentCount = 1;
					b.pAttachments = &blend_att;
					b.logicOpEnable = VK_FALSE;
					return b;
				}
				constexpr auto blend = create_blend();

				struct runtime_info
				{
					runtime_info(VkDevice d, const VkExtent2D& e) : device(d)
					{

						create_shaders(device,
						{
							"shaders/gta5_pass/ss_dir_shadow_map_blur/vert.spv",
							"shaders/gta5_pass/ss_dir_shadow_map_blur/frag.spv"
						},
						{
							VK_SHADER_STAGE_VERTEX_BIT,
							VK_SHADER_STAGE_FRAGMENT_BIT
						},
							shaders.data()
						);

						viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
						viewport.pScissors = &scissor;
						viewport.scissorCount = 1;
						viewport.pViewports = &vp;
						viewport.viewportCount = 1;
						scissor.extent = e;
						scissor.offset = { 0,0 };
						vp.height = static_cast<float>(e.height);
						vp.width = static_cast<float>(e.width);
						vp.maxDepth = 1.f;
						vp.minDepth = 0.f;
						vp.x = 0.f;
						vp.y = 0.f;
					}

					~runtime_info()
					{
						for (auto s : shaders)
							vkDestroyShaderModule(device, s.module, host_memory_manager);
					}

					void fill_create_info(VkGraphicsPipelineCreateInfo& c)
					{
						c.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
						c.basePipelineHandle = VK_NULL_HANDLE;
						c.basePipelineIndex = -1;
						c.pInputAssemblyState = &input_assembly;
						c.pMultisampleState = &multisample;
						c.pColorBlendState = &blend;
						c.pRasterizationState = &rasterizer;
						c.pStages = shaders.data();
						c.stageCount = 2;
						c.pVertexInputState = &vertex_input;
						c.pViewportState = &viewport;
						c.subpass = SUBPASS_SS_DIR_SHADOW_MAP_BLUR;
					}

					VkDevice device;
					std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {};
					VkViewport vp;
					VkRect2D scissor;
					VkPipelineViewportStateCreateInfo viewport = {};
				};
				namespace dsl
				{
					constexpr auto create_bindings()
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

						return bindings;
					}
					constexpr auto bindings = create_bindings();

					constexpr auto create_create_info()
					{
						VkDescriptorSetLayoutCreateInfo dsl = {};
						dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
						dsl.bindingCount = bindings.size();
						dsl.pBindings = bindings.data();
						return dsl;
					}
					constexpr auto create_info = create_create_info();
				}//namespace dsl
			}//namespace pipeline
		}//namespace subpass_ss_dir_shadow_map_blur

		namespace subpass_ssao_map_gen
		{
			constexpr VkAttachmentReference normal_in = { ATT_NORMAL_AO, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			constexpr VkAttachmentReference depth = { ATT_SSAO_MAP, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
			constexpr std::array<uint32_t, 3> pres = { ATT_ALBEDO_SSDS, ATT_POS_ROUGHNESS, ATT_F0_SSAO };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.pDepthStencilAttachment = &depth;
				s.inputAttachmentCount = 1;
				s.pInputAttachments = &normal_in;
				s.preserveAttachmentCount = 3;
				s.pPreserveAttachments = pres.data();
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}
			

			namespace pipeline
			{

				constexpr auto create_vertex_input()
				{
					VkPipelineVertexInputStateCreateInfo input = {};
					input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
					return input;
				}
				constexpr auto vertex_input = create_vertex_input();

				constexpr auto create_input_assembly()
				{

					VkPipelineInputAssemblyStateCreateInfo assembly = {};
					assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
					assembly.primitiveRestartEnable = VK_FALSE;
					assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
					return assembly;
				}
				constexpr auto input_assembly = create_input_assembly();

				constexpr auto create_rasterizer()
				{
					VkPipelineRasterizationStateCreateInfo r = {};
					r.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
					r.cullMode = VK_CULL_MODE_NONE;
					r.depthBiasEnable = VK_FALSE;
					r.depthClampEnable = VK_FALSE;
					r.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
					r.lineWidth = 1.f;
					r.polygonMode = VK_POLYGON_MODE_FILL;
					r.rasterizerDiscardEnable = VK_FALSE;
					return r;
				}
				constexpr auto rasterizer = create_rasterizer();

				constexpr auto create_depth()
				{
					VkPipelineDepthStencilStateCreateInfo d = {};
					d.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
					d.depthBoundsTestEnable = VK_FALSE;
					d.depthCompareOp = VK_COMPARE_OP_ALWAYS;
					d.depthTestEnable = VK_TRUE;
					d.depthWriteEnable = VK_TRUE;
					d.stencilTestEnable = VK_FALSE;
					return d;
				}
				constexpr auto depth = create_depth();

				constexpr auto create_multisample()
				{
					VkPipelineMultisampleStateCreateInfo m = {};
					m.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
					m.alphaToCoverageEnable = VK_FALSE;
					m.alphaToOneEnable = VK_FALSE;
					m.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
					m.sampleShadingEnable = VK_FALSE;
					return m;
				}
				constexpr auto multisample = create_multisample();

				struct runtime_info
				{
					runtime_info(VkDevice d, const VkExtent2D& e) : device(d)
					{

						create_shaders(device,
						{
							"shaders/gta5_pass/ssao_gen/vert.spv",
							"shaders/gta5_pass/ssao_gen/frag.spv"
						},
						{
							VK_SHADER_STAGE_VERTEX_BIT,
							VK_SHADER_STAGE_FRAGMENT_BIT
						},
							shaders.data()
						);

						viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
						viewport.pScissors = &scissor;
						viewport.scissorCount = 1;
						viewport.pViewports = &vp;
						viewport.viewportCount = 1;
						scissor.extent = e;
						scissor.offset = { 0,0 };
						vp.height = static_cast<float>(e.height);
						vp.width = static_cast<float>(e.width);
						vp.maxDepth = 1.f;
						vp.minDepth = 0.f;
						vp.x = 0.f;
						vp.y = 0.f;
					}

					~runtime_info()
					{
						for (auto s : shaders)
							vkDestroyShaderModule(device, s.module, host_memory_manager);
					}

					void fill_create_info(VkGraphicsPipelineCreateInfo& c)
					{
						c.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
						c.basePipelineHandle = VK_NULL_HANDLE;
						c.basePipelineIndex = -1;
						c.pInputAssemblyState = &input_assembly;
						c.pMultisampleState = &multisample;
						c.pRasterizationState = &rasterizer;
						c.pDepthStencilState = &depth;
						c.pStages = shaders.data();
						c.stageCount = 2;
						c.pVertexInputState = &vertex_input;
						c.pViewportState = &viewport;
						c.subpass = SUBPASS_SSAO_MAP_GEN;
					}

					VkDevice device;
					std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {};
					VkViewport vp;
					VkRect2D scissor;
					VkPipelineViewportStateCreateInfo viewport = {};
				};
				namespace dsl
				{
					constexpr auto create_bindings()
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

						return bindings;
					}
					constexpr auto bindings = create_bindings();

					constexpr auto create_create_info()
					{
						VkDescriptorSetLayoutCreateInfo dsl = {};
						dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
						dsl.bindingCount = bindings.size();
						dsl.pBindings = bindings.data();
						return dsl;
					}
					constexpr auto create_info = create_create_info();
				}//namespace dsl
			}//namespace pipeline
		}//namespace ssao_map_gen

		namespace subpass_ssao_map_blur
		{
			VkAttachmentReference ssao_out = { ATT_F0_SSAO, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			std::array<uint32_t, 3> pres = { ATT_POS_ROUGHNESS, ATT_ALBEDO_SSDS, ATT_NORMAL_AO };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &ssao_out;
				s.preserveAttachmentCount = 3;
				s.pPreserveAttachments = pres.data();
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}
			
			namespace pipeline
			{

				constexpr auto create_vertex_input()
				{
					VkPipelineVertexInputStateCreateInfo input = {};
					input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
					return input;
				}
				constexpr auto vertex_input = create_vertex_input();

				constexpr auto create_input_assembly()
				{

					VkPipelineInputAssemblyStateCreateInfo assembly = {};
					assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
					assembly.primitiveRestartEnable = VK_FALSE;
					assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
					return assembly;
				}
				constexpr auto input_assembly = create_input_assembly();

				constexpr auto create_rasterizer()
				{
					VkPipelineRasterizationStateCreateInfo r = {};
					r.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
					r.cullMode = VK_CULL_MODE_NONE;
					r.depthBiasEnable = VK_FALSE;
					r.depthClampEnable = VK_FALSE;
					r.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
					r.lineWidth = 1.f;
					r.polygonMode = VK_POLYGON_MODE_FILL;
					r.rasterizerDiscardEnable = VK_FALSE;
					return r;
				}
				constexpr auto rasterizer = create_rasterizer();

				constexpr auto create_multisample()
				{
					VkPipelineMultisampleStateCreateInfo m = {};
					m.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
					m.alphaToCoverageEnable = VK_FALSE;
					m.alphaToOneEnable = VK_FALSE;
					m.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
					m.sampleShadingEnable = VK_FALSE;
					return m;
				}
				constexpr auto multisample = create_multisample();

				constexpr auto create_blend_att()
				{
					VkPipelineColorBlendAttachmentState b = {};

					b.blendEnable = VK_FALSE;
					b.colorWriteMask = VK_COLOR_COMPONENT_A_BIT;
					return b;
				}
				constexpr auto blend_att = create_blend_att();

				constexpr auto create_blend()
				{
					VkPipelineColorBlendStateCreateInfo b = {};
					b.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
					b.attachmentCount = 1;
					b.pAttachments = &blend_att;
					b.logicOpEnable = VK_FALSE;
					return b;
				}
				constexpr auto blend = create_blend();

				struct runtime_info
				{
					runtime_info(VkDevice d, const VkExtent2D& e) : device(d)
					{

						create_shaders(device,
						{
							"shaders/gta5_pass/ssao_blur/vert.spv",
							"shaders/gta5_pass/ssao_blur/frag.spv"
						},
						{
							VK_SHADER_STAGE_VERTEX_BIT,
							VK_SHADER_STAGE_FRAGMENT_BIT
						},
							shaders.data()
						);

						viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
						viewport.pScissors = &scissor;
						viewport.scissorCount = 1;
						viewport.pViewports = &vp;
						viewport.viewportCount = 1;
						scissor.extent = e;
						scissor.offset = { 0,0 };
						vp.height = static_cast<float>(e.height);
						vp.width = static_cast<float>(e.width);
						vp.maxDepth = 1.f;
						vp.minDepth = 0.f;
						vp.x = 0.f;
						vp.y = 0.f;
					}

					~runtime_info()
					{
						for (auto s : shaders)
							vkDestroyShaderModule(device, s.module, host_memory_manager);
					}

					void fill_create_info(VkGraphicsPipelineCreateInfo& c)
					{
						c.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
						c.basePipelineHandle = VK_NULL_HANDLE;
						c.basePipelineIndex = -1;
						c.pInputAssemblyState = &input_assembly;
						c.pMultisampleState = &multisample;
						c.pRasterizationState = &rasterizer;
						c.pColorBlendState = &blend;
						c.pStages = shaders.data();
						c.stageCount = 2;
						c.pVertexInputState = &vertex_input;
						c.pViewportState = &viewport;
						c.subpass = SUBPASS_SSAO_MAP_BLUR;
					}

					VkDevice device;
					std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {};
					VkViewport vp;
					VkRect2D scissor;
					VkPipelineViewportStateCreateInfo viewport = {};
				};
				namespace dsl
				{
					constexpr auto create_binding()
					{
						VkDescriptorSetLayoutBinding binding = {};
						binding.binding = 0;
						binding.descriptorCount = 1;
						binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

						return binding;
					}
					constexpr auto binding = create_binding();

					constexpr auto create_create_info()
					{
						VkDescriptorSetLayoutCreateInfo dsl = {};
						dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
						dsl.bindingCount = 1;
						dsl.pBindings = &binding;
						return dsl;
					}
					constexpr auto create_info = create_create_info();
				}//namespace dsl
			}//namespace pipeline
		}//subpass_ssao_map_blur

		namespace subpass_image_assembler
		{
			enum REF_IN
			{
				REF_IN_POS_ROUGHNESS,
				REF_IN_F0_SSAO,
				REF_IN_ALBEDO_SSDS,
				REF_IN_NORMAL_AO,
				REF_IN_COUNT
			};

			constexpr auto create_refs_in()
			{
				std::array<VkAttachmentReference, REF_IN_COUNT> ref = {};
				ref[REF_IN_POS_ROUGHNESS].attachment = ATT_POS_ROUGHNESS;
				ref[REF_IN_POS_ROUGHNESS].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				ref[REF_IN_NORMAL_AO].attachment = ATT_NORMAL_AO;
				ref[REF_IN_NORMAL_AO].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				ref[REF_IN_ALBEDO_SSDS].attachment = ATT_ALBEDO_SSDS;
				ref[REF_IN_ALBEDO_SSDS].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				ref[REF_IN_F0_SSAO].attachment = ATT_F0_SSAO;
				ref[REF_IN_F0_SSAO].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				return ref;
			}
			constexpr std::array<VkAttachmentReference, REF_IN_COUNT>  ref_in = create_refs_in();
			constexpr VkAttachmentReference color_out = { ATT_PREIMAGE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.inputAttachmentCount = REF_IN_COUNT;
				s.pInputAttachments = ref_in.data();
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &color_out;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}


			namespace pipeline
			{

				constexpr auto create_vertex_input()
				{
					VkPipelineVertexInputStateCreateInfo input = {};
					input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
					return input;
				}
				constexpr auto vertex_input = create_vertex_input();

				constexpr auto create_input_assembly()
				{

					VkPipelineInputAssemblyStateCreateInfo assembly = {};
					assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
					assembly.primitiveRestartEnable = VK_FALSE;
					assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
					return assembly;
				}
				constexpr auto input_assembly = create_input_assembly();

				constexpr auto create_rasterizer()
				{
					VkPipelineRasterizationStateCreateInfo r = {};
					r.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
					r.cullMode = VK_CULL_MODE_NONE;
					r.depthBiasEnable = VK_FALSE;
					r.depthClampEnable = VK_FALSE;
					r.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
					r.lineWidth = 1.f;
					r.polygonMode = VK_POLYGON_MODE_FILL;
					r.rasterizerDiscardEnable = VK_FALSE;
					return r;
				}
				constexpr auto rasterizer = create_rasterizer();

				constexpr auto create_multisample()
				{
					VkPipelineMultisampleStateCreateInfo m = {};
					m.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
					m.alphaToCoverageEnable = VK_FALSE;
					m.alphaToOneEnable = VK_FALSE;
					m.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
					m.sampleShadingEnable = VK_FALSE;
					return m;
				}
				constexpr auto multisample = create_multisample();

				constexpr auto create_blend_att()
				{
					VkPipelineColorBlendAttachmentState b = {};

					b.blendEnable = VK_FALSE;
					b.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
						| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
					return b;
				}
				constexpr auto blend_att = create_blend_att();

				constexpr auto create_blend()
				{
					VkPipelineColorBlendStateCreateInfo b = {};
					b.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
					b.attachmentCount = 1;
					b.pAttachments = &blend_att;
					b.logicOpEnable = VK_FALSE;
					b.logicOp = VK_LOGIC_OP_COPY;
					return b;
				}
				constexpr auto blend = create_blend();

				struct runtime_info
				{
					runtime_info(VkDevice d, const VkExtent2D& e) : device(d)
					{

						create_shaders(device,
						{
							"shaders/gta5_pass/image_assembler/vert.spv",
							"shaders/gta5_pass/image_assembler/frag.spv"
						},
						{
							VK_SHADER_STAGE_VERTEX_BIT,
							VK_SHADER_STAGE_FRAGMENT_BIT
						},
							shaders.data()
						);

						viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
						viewport.pScissors = &scissor;
						viewport.scissorCount = 1;
						viewport.pViewports = &vp;
						viewport.viewportCount = 1;
						scissor.extent = e;
						scissor.offset = { 0,0 };
						vp.height = static_cast<float>(e.height);
						vp.width = static_cast<float>(e.width);
						vp.maxDepth = 1.f;
						vp.minDepth = 0.f;
						vp.x = 0.f;
						vp.y = 0.f;
					}

					~runtime_info()
					{
						for (auto s : shaders)
							vkDestroyShaderModule(device, s.module, host_memory_manager);
					}

					void fill_create_info(VkGraphicsPipelineCreateInfo& c)
					{
						c.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
						c.basePipelineHandle = VK_NULL_HANDLE;
						c.basePipelineIndex = -1;
						c.pInputAssemblyState = &input_assembly;
						c.pMultisampleState = &multisample;
						c.pRasterizationState = &rasterizer;
						c.pColorBlendState = &blend;
						c.pStages = shaders.data();
						c.stageCount = 2;
						c.pVertexInputState = &vertex_input;
						c.pViewportState = &viewport;
						c.subpass = SUBPASS_IMAGE_ASSEMBLER;
					}

					VkDevice device;
					std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {};
					VkViewport vp;
					VkRect2D scissor;
					VkPipelineViewportStateCreateInfo viewport = {};
				};
				namespace dsl
				{
					constexpr auto create_bindings()
					{
						std::array<VkDescriptorSetLayoutBinding, 6> bindings = {};

						for (uint32_t i = 0; i < 4; ++i)
						{
							bindings[i].binding = i;
							bindings[i].descriptorCount = 1;
							bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
							bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
						}

						bindings[4].binding =4;
						bindings[4].descriptorCount = 1;
						bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

						bindings[5].binding = 5;
						bindings[5].descriptorCount = 1;
						bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

						return bindings;
					}
					constexpr auto bindings = create_bindings();

					constexpr auto create_create_info()
					{
						VkDescriptorSetLayoutCreateInfo dsl = {};
						dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
						dsl.bindingCount = bindings.size();
						dsl.pBindings = bindings.data();
						return dsl;
					}
					constexpr auto create_info = create_create_info();
				}//namespace dsl
			}//namespace pipeline
		}//namespace subpass_image_assembler

		constexpr auto get_attachments()
		{
			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};

			atts[ATT_POS_ROUGHNESS].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_POS_ROUGHNESS].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			atts[ATT_POS_ROUGHNESS].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_POS_ROUGHNESS].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_POS_ROUGHNESS].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_POS_ROUGHNESS].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_F0_SSAO].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_F0_SSAO].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			atts[ATT_F0_SSAO].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_F0_SSAO].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_F0_SSAO].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_F0_SSAO].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_ALBEDO_SSDS].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_ALBEDO_SSDS].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			atts[ATT_ALBEDO_SSDS].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_ALBEDO_SSDS].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_ALBEDO_SSDS].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_ALBEDO_SSDS].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_NORMAL_AO].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_NORMAL_AO].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			atts[ATT_NORMAL_AO].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_NORMAL_AO].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_NORMAL_AO].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_NORMAL_AO].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_DEPTHSTENCIL].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_DEPTHSTENCIL].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			atts[ATT_DEPTHSTENCIL].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			atts[ATT_DEPTHSTENCIL].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_DEPTHSTENCIL].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			atts[ATT_DEPTHSTENCIL].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_SS_DIR_SHADOW_MAP].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_SS_DIR_SHADOW_MAP].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			atts[ATT_SS_DIR_SHADOW_MAP].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_SS_DIR_SHADOW_MAP].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_SS_DIR_SHADOW_MAP].format = VK_FORMAT_D32_SFLOAT;
			atts[ATT_SS_DIR_SHADOW_MAP].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_SSAO_MAP].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_SSAO_MAP].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			atts[ATT_SSAO_MAP].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_SSAO_MAP].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_SSAO_MAP].format = VK_FORMAT_D32_SFLOAT;
			atts[ATT_SSAO_MAP].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_PREIMAGE].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_PREIMAGE].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			atts[ATT_PREIMAGE].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_PREIMAGE].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_PREIMAGE].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_PREIMAGE].samples = VK_SAMPLE_COUNT_1_BIT;

			return atts;
		}

		constexpr auto get_dependencies()
		{
			std::array<VkSubpassDependency, DEP_COUNT> d = {};

			/*d[DEP_EXT_SSDS_GEN].srcSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_EXT_SSDS_GEN].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			d[DEP_EXT_SSDS_GEN].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			d[DEP_EXT_SSDS_GEN].dstSubpass = SUBPASS_SS_DIR_SHADOW_MAP_GEN;
			d[DEP_EXT_SSDS_GEN].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_EXT_SSDS_GEN].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;*/

			d[DEP_GBUFFER_GEN_SSDS_GEN].srcSubpass = SUBPASS_GBUFFER_GEN;
			d[DEP_GBUFFER_GEN_SSDS_GEN].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_GBUFFER_GEN_SSDS_GEN].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_GBUFFER_GEN_SSDS_GEN].dstSubpass = SUBPASS_SS_DIR_SHADOW_MAP_GEN;
			d[DEP_GBUFFER_GEN_SSDS_GEN].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_GBUFFER_GEN_SSDS_GEN].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

			d[DEP_GBUFFER_GEN_SSAO_GEN].srcSubpass = SUBPASS_GBUFFER_GEN;
			d[DEP_GBUFFER_GEN_SSAO_GEN].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_GBUFFER_GEN_SSAO_GEN].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_GBUFFER_GEN_SSAO_GEN].dstSubpass = SUBPASS_SSAO_MAP_GEN;
			d[DEP_GBUFFER_GEN_SSAO_GEN].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_GBUFFER_GEN_SSAO_GEN].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

			d[DEP_SSDS_GEN_SELF].srcSubpass = SUBPASS_SS_DIR_SHADOW_MAP_GEN;
			d[DEP_SSDS_GEN_SELF].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			d[DEP_SSDS_GEN_SELF].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			d[DEP_SSDS_GEN_SELF].dstSubpass = SUBPASS_SS_DIR_SHADOW_MAP_GEN;
			d[DEP_SSDS_GEN_SELF].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_SSDS_GEN_SELF].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			d[DEP_SSAO_GEN_SELF].srcSubpass = SUBPASS_SSAO_MAP_GEN;
			d[DEP_SSAO_GEN_SELF].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			d[DEP_SSAO_GEN_SELF].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			d[DEP_SSAO_GEN_SELF].dstSubpass = SUBPASS_SSAO_MAP_GEN;
			d[DEP_SSAO_GEN_SELF].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_SSAO_GEN_SELF].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].srcSubpass = SUBPASS_SS_DIR_SHADOW_MAP_BLUR;
			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].dstSubpass = SUBPASS_IMAGE_ASSEMBLER;
			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].srcSubpass = SUBPASS_SSAO_MAP_BLUR;
			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].dstSubpass = SUBPASS_IMAGE_ASSEMBLER;
			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

			d[DEP_GBUFFER_GEN_IMAGE_ASSEMBLER].srcSubpass = SUBPASS_GBUFFER_GEN;
			d[DEP_GBUFFER_GEN_IMAGE_ASSEMBLER].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_GBUFFER_GEN_IMAGE_ASSEMBLER].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_GBUFFER_GEN_IMAGE_ASSEMBLER].dstSubpass = SUBPASS_IMAGE_ASSEMBLER;
			d[DEP_GBUFFER_GEN_IMAGE_ASSEMBLER].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_GBUFFER_GEN_IMAGE_ASSEMBLER].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

			d[DEP_GBUFFER_GEN_SSAO_BLUR].srcSubpass = SUBPASS_GBUFFER_GEN;
			d[DEP_GBUFFER_GEN_SSAO_BLUR].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_GBUFFER_GEN_SSAO_BLUR].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_GBUFFER_GEN_SSAO_BLUR].dstSubpass = SUBPASS_SSAO_MAP_BLUR;
			d[DEP_GBUFFER_GEN_SSAO_BLUR].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_GBUFFER_GEN_SSAO_BLUR].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

			d[DEP_GBUFFER_GEN_SSDS_BLUR].srcSubpass = SUBPASS_GBUFFER_GEN;
			d[DEP_GBUFFER_GEN_SSDS_BLUR].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_GBUFFER_GEN_SSDS_BLUR].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_GBUFFER_GEN_SSDS_BLUR].dstSubpass = SUBPASS_SS_DIR_SHADOW_MAP_BLUR;
			d[DEP_GBUFFER_GEN_SSDS_BLUR].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_GBUFFER_GEN_SSDS_BLUR].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

			/*d[DEP_END].srcSubpass = SUBPASS_IMAGE_ASSEMBLER;
			d[DEP_END].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_END].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_END].dstSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_END].dstStageMask = VK_PIPELINE_STAGE_;
			d[DEP_END].dstAccessMask = VK_ACCESS_PR;*/

			return d;
		}

		constexpr std::array<VkSubpassDependency, DEP_COUNT> deps=get_dependencies();
		constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts = get_attachments();
		constexpr std::array<VkSubpassDescription, SUBPASS_COUNT> subpasses = 
		{
			subpass_gbuffer_gen::create_subpass(),
			subpass_ss_dir_shadow_map_gen::create_subpass(),
			subpass_ss_dir_shadow_map_blur::create_subpass(),
			subpass_ssao_map_gen::create_subpass(),
			subpass_ssao_map_blur::create_subpass(),
			subpass_image_assembler::create_subpass()
		};

		constexpr VkRenderPassCreateInfo create_create_info()
		{
			VkRenderPassCreateInfo pass = {};

			pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			pass.attachmentCount = ATT_COUNT;
			pass.dependencyCount = DEP_COUNT;
			pass.pAttachments = atts.data();
			pass.subpassCount = SUBPASS_COUNT;
			pass.pSubpasses = subpasses.data();
			pass.pDependencies = deps.data();

			return pass;
		}

		constexpr VkRenderPassCreateInfo create_info=create_create_info();
	}//namespace render_pass_frame_image_gen

	namespace render_pass_postprocessing
	{
		enum ATT
		{
			ATT_SWAP_CHAIN_IMAGE,
			ATT_COUNT
		};
		enum DEP
		{
			DEP_BEGIN,
			DEP_COUNT
		};

		namespace subpass_bypass
		{
			VkAttachmentReference swap_chain_image = { ATT_SWAP_CHAIN_IMAGE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &swap_chain_image;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			namespace pipeline
			{

				constexpr auto create_vertex_input()
				{
					VkPipelineVertexInputStateCreateInfo input = {};
					input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
					return input;
				}
				constexpr auto vertex_input = create_vertex_input();

				constexpr auto create_input_assembly()
				{

					VkPipelineInputAssemblyStateCreateInfo assembly = {};
					assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
					assembly.primitiveRestartEnable = VK_FALSE;
					assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
					return assembly;
				}
				constexpr auto input_assembly = create_input_assembly();

				constexpr auto create_rasterizer()
				{
					VkPipelineRasterizationStateCreateInfo r = {};
					r.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
					r.cullMode = VK_CULL_MODE_NONE;
					r.depthBiasEnable = VK_FALSE;
					r.depthClampEnable = VK_FALSE;
					r.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
					r.lineWidth = 1.f;
					r.polygonMode = VK_POLYGON_MODE_FILL;
					r.rasterizerDiscardEnable = VK_FALSE;
					return r;
				}
				constexpr auto rasterizer = create_rasterizer();

				constexpr auto create_multisample()
				{
					VkPipelineMultisampleStateCreateInfo m = {};
					m.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
					m.alphaToCoverageEnable = VK_FALSE;
					m.alphaToOneEnable = VK_FALSE;
					m.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
					m.sampleShadingEnable = VK_FALSE;
					return m;
				}
				constexpr auto multisample = create_multisample();

				constexpr auto create_blend_att()
				{
					VkPipelineColorBlendAttachmentState b = {};

					b.blendEnable = VK_FALSE;
					b.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
						VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
					return b;
				}
				constexpr auto blend_att = create_blend_att();

				constexpr auto create_blend()
				{
					VkPipelineColorBlendStateCreateInfo b = {};
					b.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
					b.attachmentCount = 1;
					b.pAttachments = &blend_att;
					b.logicOpEnable = VK_FALSE;
					return b;
				}
				constexpr auto blend = create_blend();

				struct runtime_info
				{
					runtime_info(VkDevice d, const VkExtent2D& e) : device(d)
					{

						create_shaders(device,
						{
							"shaders/gta5_pass/postprocessing/vert.spv",
							"shaders/gta5_pass/postprocessing/frag.spv"
						},
						{
							VK_SHADER_STAGE_VERTEX_BIT,
							VK_SHADER_STAGE_FRAGMENT_BIT
						},
							shaders.data()
						);

						viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
						viewport.pScissors = &scissor;
						viewport.scissorCount = 1;
						viewport.pViewports = &vp;
						viewport.viewportCount = 1;
						scissor.extent = e;
						scissor.offset = { 0,0 };
						vp.height = static_cast<float>(e.height);
						vp.width = static_cast<float>(e.width);
						vp.maxDepth = 1.f;
						vp.minDepth = 0.f;
						vp.x = 0.f;
						vp.y = 0.f;
					}

					~runtime_info()
					{
						for (auto s : shaders)
							vkDestroyShaderModule(device, s.module, host_memory_manager);
					}

					void fill_create_info(VkGraphicsPipelineCreateInfo& c)
					{
						c.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
						c.basePipelineHandle = VK_NULL_HANDLE;
						c.basePipelineIndex = -1;
						c.pInputAssemblyState = &input_assembly;
						c.pMultisampleState = &multisample;
						c.pRasterizationState = &rasterizer;
						c.pColorBlendState = &blend;
						c.pStages = shaders.data();
						c.stageCount = 2;
						c.pVertexInputState = &vertex_input;
						c.pViewportState = &viewport;
						c.subpass = 0;
					}

					VkDevice device;
					std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {};
					VkViewport vp;
					VkRect2D scissor;
					VkPipelineViewportStateCreateInfo viewport = {};
				};
				namespace dsl
				{
					constexpr auto create_binding()
					{
						VkDescriptorSetLayoutBinding binding = {};
						binding.binding = 0;
						binding.descriptorCount = 1;
						binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

						return binding;
					}
					constexpr auto binding = create_binding();

					constexpr auto create_create_info()
					{
						VkDescriptorSetLayoutCreateInfo dsl = {};
						dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
						dsl.bindingCount = 1;
						dsl.pBindings = &binding;
						return dsl;
					}
					constexpr auto create_info = create_create_info();
				}//namespace dsl
			}//namespace pipeline


		}// namespace subpass_bypass

		constexpr std::array<VkAttachmentDescription, ATT_COUNT> get_attachments()
		{

			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};
			atts[ATT_SWAP_CHAIN_IMAGE].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_SWAP_CHAIN_IMAGE].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			atts[ATT_SWAP_CHAIN_IMAGE].format = VK_FORMAT_B8G8R8A8_UNORM;
			atts[ATT_SWAP_CHAIN_IMAGE].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_SWAP_CHAIN_IMAGE].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_SWAP_CHAIN_IMAGE].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_SWAP_CHAIN_IMAGE].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_SWAP_CHAIN_IMAGE].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			return atts;
		}

		constexpr auto get_dependencies()
		{
			std::array<VkSubpassDependency, DEP_COUNT> d = {};
			d[DEP_BEGIN].srcSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_BEGIN].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_BEGIN].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_BEGIN].dstSubpass = 0;
			d[DEP_BEGIN].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_BEGIN].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			return d;
		}

		constexpr std::array<VkSubpassDependency, DEP_COUNT> deps = get_dependencies();
		constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts = get_attachments();
		constexpr VkSubpassDescription sp_bypass = subpass_bypass::create_subpass();

		constexpr VkRenderPassCreateInfo create_create_info()
		{
			VkRenderPassCreateInfo pass = {};
			pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			pass.attachmentCount = ATT_COUNT;
			pass.dependencyCount = DEP_COUNT;
			pass.pAttachments = atts.data();
			pass.subpassCount = 1;
			pass.pSubpasses = &sp_bypass;
			pass.pDependencies = deps.data();
			return pass;
		}
		constexpr VkRenderPassCreateInfo create_info = create_create_info();

	}// namespace render_pass_postprocessing

	namespace dp
	{
		constexpr auto create_sizes()
		{
			std::array<VkDescriptorPoolSize, 3> sizes = {};
			sizes[0].descriptorCount = 6;// ub_count;
			sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			sizes[1].descriptorCount = 7;// cis_count;
			sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			sizes[2].descriptorCount = 7;// ia_count;
			sizes[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			return sizes;
		}
		constexpr auto sizes = create_sizes();

		constexpr auto create_create_info()
		{
			VkDescriptorPoolCreateInfo pool = {};
			pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool.maxSets = GP_COUNT;
			pool.poolSizeCount = sizes.size();
			pool.pPoolSizes = sizes.data();
			return pool;
		}
		constexpr auto create_info = create_create_info();
	}
}