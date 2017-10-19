#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_SWIZZLE
#include<glm\glm.hpp>
#include <glm\gtx\hash.hpp>
#include <glm\gtx\transform.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>

#include <string>
#include <vector>
#include <stack>
#include <map>
#include <queue>
#include <stdexcept>
#include <chrono>
#include <array>
#include <set>
#include <mutex>
#include <memory>
#include <thread>
#include <future>
#include <optional>
#include <tuple>
#include <bitset>
#include <iostream>
#include <utility>


namespace rcq
{
	//interface related

	enum TEX_TYPE
	{
		TEX_TYPE_COLOR,
		TEX_TYPE_ROUGHNESS,
		TEX_TYPE_METAL,
		TEX_TYPE_NORMAL,
		TEX_TYPE_HEIGHT,
		TEX_TYPE_AO,
		TEX_TYPE_COUNT
	};

	enum TEX_TYPE_FLAG : uint32_t
	{
		TEX_TYPE_FLAG_COLOR = 1,
		TEX_TYPE_FLAG_ROUGHNESS = 2,
		TEX_TYPE_FLAG_METAL=4,
		TEX_TYPE_FLAG_NORMAL=8,
		TEX_TYPE_FLAG_HEIGHT=16,
		TEX_TYPE_FLAG_AO=32
	};

	enum LIGHT_FLAG : uint32_t
	{
		LIGHT_FLAG_SHADOW_MAP=1
	};

	enum USAGE
	{
		USAGE_STATIC,
		USAGE_DYNAMIC,
		USAGE_COUNT
	};

	enum LIFE_EXPECTANCY
	{
		LIFE_EXPECTANCY_LONG,
		LIFE_EXPECTANCY_SHORT,
		LIFE_EXPECTANCY_COUNT
	};

	enum RENDERABLE_TYPE
	{
		RENDERABLE_TYPE_MAT_OPAQUE,
		RENDERABLE_TYPE_MAT_EM,
		RENDERABLE_TYPE_LIGHT_OMNI,
		RENDERABLE_TYPE_SKYBOX,
		RENDERABLE_TYPE_COUNT
	};

	enum RENDER_ENGINE
	{
		RENDER_ENGINE_GTA5,
		RENDER_ENGINE_COUNT
	};

	enum RESOURCE_TYPE
	{
		RESOURCE_TYPE_MAT_OPAQUE,
		RESOURCE_TYPE_MAT_EM,
		RESOURCE_TYPE_LIGHT_OMNI,
		RESOURCE_TYPE_SKYBOX,
		RESOURCE_TYPE_MESH,
		RESOURCE_TYPE_TR,		
		RESOURCE_TYPE_MEMORY,
		RESOURCE_TYPE_COUNT
	};

	enum SAMPLER_TYPE
	{
		SAMPLER_TYPE_SIMPLE,
		SAMPLER_TYPE_CUBE,
		SAMPLER_TYPE_COUNT
	};

	enum DESCRIPTOR_SET_LAYOUT_TYPE
	{
		DESCRIPTOR_SET_LAYOUT_TYPE_MAT_OPAQUE,
		DESCRIPTOR_SET_LAYOUT_TYPE_MAT_EM,
		DESCRIPTOR_SET_LAYOUT_TYPE_TR,
		DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI,
		DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX,
		DESCRIPTOR_SET_LAYOUT_TYPE_COUNT
	};

	struct material_opaque_data;
	struct transform_data;
	struct camera_data;
	struct light_omni_data;
	struct light_spot_data;
	struct light_dir_data;

	

	typedef std::array<std::string, TEX_TYPE_COUNT> texfiles;
	typedef size_t unique_id;


	typedef std::tuple<unique_id, material_opaque_data, texfiles> build_mat_opaque_info;
	enum 
	{
		BUILD_MAT_OPAQUE_INFO_MAT_ID,
		BUILD_MAT_OPAQUE_INFO_MAT_DATA,
		BUILD_MAT_OPAQUE_INFO_TEXINFOS,
	};

	typedef std::tuple<unique_id, std::string> build_mat_em_info;
	enum
	{
		BUILD_MAT_EM_INFO_ID,
		BUILD_MAT_EM_INFO_STRING
	};

	typedef std::tuple<unique_id, std::string, bool> build_mesh_info;
	enum
	{
		BUILD_MESH_INFO_MESH_ID,
		BUILD_MESH_INFO_FILENAME,
		BUILD_MESH_INFO_CALC_TB  //calculate tangent and bitangent vectors

	};

	typedef std::tuple<unique_id, transform_data, USAGE> build_tr_info;
	enum
	{
		BUILD_TR_INFO_TR_ID,
		BUILD_TR_INFO_TR_DATA,
		BUILD_TR_INFO_USAGE
	};

	typedef std::tuple<unique_id, light_omni_data, USAGE> build_light_omni_info; 
	enum
	{
		BUILD_LIGHT_OMNI_INFO_ID,
		BUILD_LIGHT_OMNI_INFO_DATA,
		BUILD_LIGHT_OMNI_INFO_USAGE
	};  

	typedef std::tuple<unique_id, std::string> build_skybox_info;
	enum
	{
		BUILD_SKYBOX_INFO_ID,
		BUILD_SKYBOX_INFO_FILENAME
	};

	typedef std::tuple<unique_id, std::vector<VkMemoryAllocateInfo>> build_memory_info;

	typedef std::tuple<unique_id, unique_id, unique_id, unique_id> build_renderable_info;
	enum
	{
		BUILD_RENDERABLE_INFO_RENDERABLE_ID,
		BUILD_RENDERABLE_INFO_TR_ID,
		BUILD_RENDERABLE_INFO_MESH_ID,
		BUILD_RENDERABLE_INFO_MAT_OR_LIGHT_ID
	};

	typedef std::tuple<unique_id, transform_data> update_tr_info;
	enum
	{
		UPDATE_TR_INFO_TR_ID,
		UPDATE_TR_INFO_TR_DATA
	};


	struct material_opaque_data
	{
		glm::vec3 color;
		float roughness;
		float metal;
		float height_scale;
		uint32_t flags;
	};

	struct light_omni_data
	{
		glm::vec3 pos;
		uint32_t flags;
		glm::vec3 radiance;
	};

	/*struct spotlight_data
	{
		glm::vec3 pos;
		float angle;
		glm::vec3 dir;
		float penumbra_angle;
		glm::vec3 radiance;
		float umbra_angle;
	};*/

	struct dir_light_data
	{
		glm::vec3 dir;
		uint32_t flags;
		glm::vec3 irradiance;
	};

	struct transform_data
	{
		glm::mat4 model;
		glm::vec3 scale;
	private:
		uint32_t padding0;
	public:
		glm::vec2 tex_scale;
	private:
		uint32_t padding1[2];
	};

	struct camera_data
	{
		glm::mat4 proj_x_view;
		glm::vec3 pos;
	private:
		uint32_t padding0;
	};


	//engine related


	std::vector<char> read_file(const std::string_view& filename);

	VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code);
	VkPipelineLayout create_layout(VkDevice device, const std::vector<VkDescriptorSetLayout>& dsls);
	void create_shaders(VkDevice device, const std::vector<std::string_view>& files, const std::vector<VkShaderStageFlagBits>& stages,
		VkPipelineShaderStageCreateInfo* shaders);

	VkFormat find_support_format(VkPhysicalDevice device, const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features);

	uint32_t find_memory_type(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties);

	struct base_info;
	void create_staging_buffer(const base_info& base, VkDeviceSize size, VkBuffer & buffer, VkDeviceMemory & memory);

	VkCommandBuffer begin_single_time_command(VkDevice device, VkCommandPool command_pool);
	void end_single_time_command_buffer(VkDevice device, VkCommandPool cp, VkQueue queue_for_submit, VkCommandBuffer cb);


	inline VkFormat find_depth_format(VkPhysicalDevice device)
	{
		return find_support_format(device, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	template<size_t... indices, typename Callable, typename Tuple>
	inline constexpr void for_each_impl(Callable&& f, Tuple&& tuple, std::index_sequence<indices...>)
	{
		auto l = { (f.operator()<indices>(std::get<indices>(tuple)), 0)... };
	}

	template<typename Callable, typename Tuple>
	inline constexpr void for_each(Callable&& f, Tuple&& tuple)
	{
		for_each_impl(std::forward<Callable>(f), std::forward<Tuple>(tuple), std::make_index_sequence<std::tuple_size<Tuple>::value>());
	}

	template<typename T>
	constexpr inline T calc_offset(T alignment, T raw_offset)
	{
		static_assert(std::is_integral_v<T>);
		return ((raw_offset+alignment-1)/alignment)*alignment;
	}

	extern const VkAllocationCallbacks* host_memory_manager;
	extern const uint32_t OMNI_SHADOW_MAP_SIZE;

	struct base_create_info
	{
		std::vector<const char*> validation_layers;
		std::vector<const char*> instance_extensions;
		std::vector<const char*> device_extensions;
		int width;
		int height;
		const char* window_name;
		bool enable_validation_layers;
		VkPhysicalDeviceFeatures device_features;
	};

	struct queue_family_indices
	{
		int graphics_family = -1;
		int present_family = -1;

		bool complete()
		{
			return graphics_family >= 0 && present_family >= 0;
		}
	};

	struct swap_chain_support_details
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	struct base_info
	{
		VkDevice device;
		std::vector<VkImageView> swap_chain_image_views;
		VkSwapchainKHR swap_chain;
		VkFormat swap_chain_image_format;
		VkExtent2D swap_chain_image_extent;
		VkPhysicalDevice physical_device;
		queue_family_indices queue_families;
		VkQueue graphics_queue;
		VkQueue present_queue;
		GLFWwindow* window;
		VkFormatProperties format_properties;
	};

	struct texture
	{
		VkImage image;
		VkImageView view;
		VkDeviceMemory memory;
		SAMPLER_TYPE sampler_type;
	};

	typedef std::array<texture, TEX_TYPE_COUNT> mat_opaque_texs;
	typedef std::pair<uint32_t, size_t> cell_info; //block id, offset

	struct material_opaque
	{
		mat_opaque_texs texs;
		VkDescriptorSet ds;
		uint32_t pool_index;
		VkBuffer data;
		cell_info cell;
	};

	struct material_em
	{
		VkDescriptorSet ds;
	};

	struct mesh
	{
		VkBuffer vb; //vertex
		VkBuffer ib; //index
		VkBuffer veb; //vertex ext
		VkDeviceMemory memory;
		uint32_t size;
	};



	struct transform
	{
		VkDescriptorSet ds;
		uint32_t pool_index;
		USAGE usage;
		VkBuffer buffer;
		void* data;
		cell_info cell;
	};

	struct render_settings
	{
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec3 pos;
		float near;
		float far;

		glm::vec3 light_dir;
		glm::vec3 irradiance;
		glm::vec3 ambient_irradiance;
	};

	struct light_omni
	{
		VkDescriptorSet ds;
		uint32_t pool_index;
		USAGE usage;
		VkBuffer buffer;
		void* data;
		cell_info cell;
		texture shadow_map;
		VkFramebuffer shadow_map_fb;
	};
	struct light_dir
	{
		VkDescriptorSet ds;
		uint32_t pool_index;
		USAGE usage;
		VkBuffer buffer;
		void* data;
		cell_info cell;
		texture shadow_map;
		VkFramebuffer shadow_map_fb;
	};

	struct skybox
	{
		texture tex;
		VkDescriptorSet ds;
		uint32_t pool_index;
	};

	
	struct renderable
	{
		unique_id id;
		mesh m;
		VkDescriptorSet tr_ds;
		VkDescriptorSet mat_light_ds;
		bool destroy = false;

		//for lights
		texture shadow_map;
		VkFramebuffer shadow_map_fb;
	};

	typedef std::array<std::vector<renderable>, RENDERABLE_TYPE_COUNT> renderable_container;

	struct core_package
	{
		std::array<std::vector<build_renderable_info>, RENDERABLE_TYPE_COUNT> build_renderable;
		std::array<std::vector<unique_id>, RENDERABLE_TYPE_COUNT> destroy_renderable;
		std::vector<update_tr_info> update_tr;
		std::optional<std::promise<void>> confirm_destroy;
		render_settings settings;
		bool render = false;
	};


	typedef std::vector<VkDeviceMemory> memory;

	typedef std::tuple<
		std::vector<build_mat_opaque_info>,
		std::vector<build_mat_em_info>,
		std::vector<build_light_omni_info>,
		std::vector<build_skybox_info>,
		std::vector<build_mesh_info>, 
		std::vector<build_tr_info>, 
		std::vector<build_memory_info>
	> build_package;

	typedef std::array<std::vector<unique_id>, RESOURCE_TYPE_COUNT> destroy_ids;

	struct destroy_package
	{
		destroy_ids ids;
		std::optional<std::future<void>> destroy_confirmation;
	};

	/*struct command_package
	{
		std::optional<core_package> core_p;
		std::optional<build_package> resource_manager_build_p;
		std::optional<destroy_package> resource_mananger_destroy_p;
	};*/

	typedef std::packaged_task<material_opaque()> build_mat_opaque_task;
	typedef std::packaged_task<material_em()> build_mat_em_task;
	typedef std::packaged_task<light_omni()> build_light_omni_task;
	typedef std::packaged_task<skybox()> build_skybox_task;
	typedef std::packaged_task<mesh()> build_mesh_task;
	typedef std::packaged_task<transform()> build_tr_task;
	typedef std::packaged_task<memory()> build_memory_task;

	typedef std::tuple<
		std::vector<build_mat_opaque_task>,
		std::vector<build_mat_em_task>,
		std::vector<build_light_omni_task>,
		std::vector<build_skybox_task>,
		std::vector<build_mesh_task>,
		std::vector<build_tr_task>, 
		std::vector<build_memory_task>
	> build_task_package;


	template<size_t res_type>
	struct resource_typename;

	template<> struct resource_typename<RESOURCE_TYPE_MAT_OPAQUE> { typedef material_opaque type; };
	template<> struct resource_typename<RESOURCE_TYPE_MAT_EM> { typedef material_em type; };
	template<> struct resource_typename<RESOURCE_TYPE_MESH> { typedef mesh type; };
	template<> struct resource_typename<RESOURCE_TYPE_TR>{ typedef transform type; };
	template<> struct resource_typename<RESOURCE_TYPE_MEMORY> { typedef memory type; };
	template<> struct resource_typename<RESOURCE_TYPE_LIGHT_OMNI> { typedef light_omni type; };
	template<> struct resource_typename<RESOURCE_TYPE_SKYBOX> { typedef skybox type; };

	typedef uint32_t pool_id;

	struct descriptor_pool_pool
	{
		std::vector<VkDescriptorPool> pools;
		std::vector<uint32_t> availability;
		pool_id get_available_pool_id()
		{
			pool_id id= 0;
			for (uint32_t av : availability)
			{
				if (av > 0)
					return id;
				++id;
			}
			return id;
		}
	};

	class basic_pass;
	class omni_light_shadow_pass;
	template<size_t res_type>
	struct render_pass_typename {};

	/*template<> struct render_pass_typename<RENDER_PASS_BASIC> { typedef  basic_pass type; };
	template<> struct render_pass_typename<RENDER_PASS_OMNI_LIGHT_SHADOW> { typedef omni_light_shadow_pass type; };*/

	/*struct graphics_pipeline_create_info
	{
		std::vector<VkShaderModule> shader_modules = {};
		std::vector<VkPipelineShaderStageCreateInfo> shaders = {};
		std::vector<VkVertexInputAttributeDescription> vertex_attribs = {};
		std::vector<VkVertexInputBindingDescription> vertex_bindings = {};
		VkPipelineVertexInputStateCreateInfo vertex_input = {};
		VkPipelineInputAssemblyStateCreateInfo assembly = {};
		std::vector<VkViewport> vps = {};
		std::vector<VkRect2D> scissors = {};
		VkPipelineViewportStateCreateInfo viewport = {};
		VkPipelineDepthStencilStateCreateInfo depthstencil = {};
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		VkPipelineMultisampleStateCreateInfo multisample = {};
		std::vector<VkPipelineColorBlendAttachmentState> color_attachnemts = {};
		VkPipelineColorBlendStateCreateInfo blend = {};
		std::vector<VkDescriptorSetLayout> dsls = {};
		VkPipelineLayout layout = {};
		VkDevice device=VK_NULL_HANDLE;
		bool has_depthstencil = false;
		bool has_blend = false;
		uint64_t sample_mask=0;

		constexpr graphics_pipeline_create_info()
		{
			vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			depthstencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		}
		void set_device(VkDevice d)
		{
			device = d;
		}
		void create_shader_modules(const std::vector<std::string_view>& filenames)
		{
			shader_modules.resize(filenames.size());
			for (uint32_t i = 0; i < shader_modules.size(); ++i)
			{
				auto code = read_file(filenames[i]);
				VkShaderModuleCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				info.codeSize = code.size();
				info.pCode = reinterpret_cast<const uint32_t*>(code.data());

				if (vkCreateShaderModule(device, &info, rcq::host_memory_manager, &shader_modules[i]) != VK_SUCCESS)
					throw std::runtime_error("failed to create shader module!");
			}
		}
		void destroy_modules()
		{
			for (auto mod : shader_modules)
				vkDestroyShaderModule(device, mod, host_memory_manager);
		}
		void fill_shader_create_infos(const std::vector<VkShaderStageFlagBits>& stages)
		{
			assert(stages.size() == shader_modules.size());
			shaders.resize(shader_modules.size());
			for (uint32_t i = 0; i < stages.size(); ++i)
			{
				shaders[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				shaders[i].module = shader_modules[i];
				shaders[i].pName = "main";
				shaders[i].stage = stages[i];
			}
		}
		void add_vertex_binding(const VkVertexInputBindingDescription& binding)
		{
			vertex_bindings.push_back(binding);
		}
		void add_vertex_attrib(const VkVertexInputAttributeDescription& attrib)
		{
			vertex_attribs.push_back(attrib);
		}
		void set_vertex_input_pointers()
		{
			if (!vertex_bindings.empty())
			{
				vertex_input.pVertexBindingDescriptions = vertex_bindings.data();
				vertex_input.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_bindings.size());
			}
			if (!vertex_attribs.empty())
			{
				vertex_input.pVertexAttributeDescriptions = vertex_attribs.data();
				vertex_input.vertexAttributeDescriptionCount = vertex_attribs.size();
			}
		}
		void fill_assembly(VkBool32 primitive_restart_enable, VkPrimitiveTopology topology)
		{
			assembly.primitiveRestartEnable = primitive_restart_enable;
			assembly.topology = topology;
		}
		void set_default_viewport(uint32_t width, uint32_t height)
		{
			VkViewport vp;
			vp.height = static_cast<float>(height);
			vp.width = static_cast<float>(width);
			vp.x = 0.f;
			vp.y = 0.f;
			vp.minDepth = 0.f;
			vp.maxDepth = 1.f;
			vps.push_back(vp);

			VkRect2D scissor;
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset = { 0,0 };
			scissors.push_back(scissor);
		}
		void set_viewport_pointers()
		{
			viewport.pViewports = vps.data();
			viewport.viewportCount = vps.size();
			viewport.pScissors = scissors.data();
			viewport.scissorCount = scissors.size();
		}
		void fill_rasterizer(VkBool32 depth_clamp_enable, VkBool32 rasterization_discard_enable, VkPolygonMode polygon_mode,
			VkCullModeFlags cull_mode, VkFrontFace front_face, VkBool32 depth_bias_enable, float depth_bias_constant_factor,
			float depth_bias_clamp, float depth_bias_slope_factor)
		{
			rasterizer.depthClampEnable = depth_clamp_enable;
			rasterizer.rasterizerDiscardEnable = rasterization_discard_enable;
			rasterizer.polygonMode = polygon_mode;
			rasterizer.cullMode = cull_mode;
			rasterizer.frontFace = front_face;
			rasterizer.depthBiasEnable = depth_bias_enable;
			rasterizer.depthBiasConstantFactor = depth_bias_constant_factor;
			rasterizer.depthBiasClamp = depth_bias_clamp;
			rasterizer.depthBiasSlopeFactor = depth_bias_slope_factor;
		}
		void fill_depthstencil(VkBool32 depth_test_enable, VkBool32 depth_write_enable, VkCompareOp depth_compare_op,
			VkBool32 depth_bounds_test_enable, VkBool32 stencil_test_enable, std::optional<VkStencilOpState> stencil_op_state_front,
			std::optional<VkStencilOpState> stencil_op_state_back, float min_depth_bounds, float max_depth_bounds)
		{
			has_depthstencil = true;
			depthstencil.depthTestEnable = depth_test_enable;
			depthstencil.depthWriteEnable = depth_write_enable;
			depthstencil.depthCompareOp = depth_compare_op;
			depthstencil.depthBoundsTestEnable = depth_bounds_test_enable;
			depthstencil.stencilTestEnable = stencil_test_enable;
			if (stencil_op_state_front)
				depthstencil.front = stencil_op_state_front.value();
			if (stencil_op_state_back)
				depthstencil.back = stencil_op_state_back.value();
			depthstencil.minDepthBounds = min_depth_bounds;
			depthstencil.maxDepthBounds = max_depth_bounds;
		}
		void fill_multisample(VkSampleCountFlagBits rasterization_samples, VkBool32 sample_shading_enable, float min_sample_shading,
			uint64_t sample_mask, VkBool32 alpha_to_coverage_enable, VkBool32 alpha_to_one_enable)
		{
			multisample.rasterizationSamples = rasterization_samples;
			multisample.sampleShadingEnable = sample_shading_enable;
			multisample.minSampleShading = min_sample_shading;
			this->sample_mask = sample_mask;
			multisample.pSampleMask = reinterpret_cast<uint32_t*>(&sample_mask);
			multisample.alphaToCoverageEnable = alpha_to_coverage_enable;
			multisample.alphaToOneEnable = alpha_to_one_enable;
		}
		void add_color_attachment(const VkPipelineColorBlendAttachmentState& att)
		{
			color_attachnemts.push_back(att);
		}
		void fill_blend(VkBool32 logic_op_enable, VkLogicOp logic_op)
		{
			has_blend = true;
			blend.logicOpEnable = logic_op_enable;
			blend.logicOp = logic_op;
		}
		void set_blend_pointers()
		{
			blend.pAttachments = color_attachnemts.data();
			blend.attachmentCount = color_attachnemts.size();
		}
		void add_dsl(VkDescriptorSetLayout dsl)
		{
			dsls.push_back(dsl);
		}
		VkPipelineLayout create_layout()
		{
			VkPipelineLayoutCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			info.pSetLayouts = dsls.data();
			info.setLayoutCount = dsls.size();

			if (vkCreatePipelineLayout(device, &info, host_memory_manager, &layout) != VK_SUCCESS)
				throw std::runtime_error("failed to create pipeline layout!");
			return layout;
		}
		void fill_create_info(VkGraphicsPipelineCreateInfo& create, VkRenderPass render_pass, uint32_t subpass, 
			VkPipeline base_pipeline_handle, int32_t base_pipeline_index)
		{
			create.basePipelineHandle = base_pipeline_handle;
			create.basePipelineIndex = base_pipeline_index;
			create.renderPass = render_pass;
			create.subpass = subpass;
		}
		void set_create_info_pointers(VkGraphicsPipelineCreateInfo& create)
		{
			create.pInputAssemblyState = &assembly;
			create.pVertexInputState = &vertex_input;
			create.pViewportState = &viewport;
			create.pRasterizationState = &rasterizer;
			create.pMultisampleState = &multisample;
			create.pStages = shaders.data();
			create.stageCount = shaders.size();
			if (has_depthstencil)
				create.pDepthStencilState = &depthstencil;
			if (has_blend)
				create.pColorBlendState = &blend;
		}
	};*/


	struct vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 tex_coord;

		constexpr static VkVertexInputBindingDescription get_binding_description()
		{
			VkVertexInputBindingDescription binding_description = {};
			binding_description.binding = 0;
			binding_description.stride = sizeof(vertex);
			binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return binding_description;
		}

		constexpr static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions()
		{
			std::array<VkVertexInputAttributeDescription, 3> attribute_description = {};
			attribute_description[0].binding = 0;
			attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_description[0].location = 0;
			attribute_description[0].offset = offsetof(vertex, pos);

			attribute_description[1].binding = 0;
			attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_description[1].location = 1;
			attribute_description[1].offset = offsetof(vertex, normal);

			attribute_description[2].binding = 0;
			attribute_description[2].format = VK_FORMAT_R32G32_SFLOAT;
			attribute_description[2].location = 2;
			attribute_description[2].offset = offsetof(vertex, tex_coord);

			return attribute_description;
		}


		bool operator==(const vertex& other) const
		{
			return pos == other.pos && normal == other.normal && tex_coord == other.tex_coord;
		}
	};

	struct vertex_ext
	{
		glm::vec3 tangent;
		glm::vec3 bitangent;

		constexpr static VkVertexInputBindingDescription get_binding_description()
		{
			VkVertexInputBindingDescription binding_description = {};
			binding_description.binding = 1;
			binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			binding_description.stride = sizeof(vertex_ext);
			return binding_description;
		}

		constexpr static std::array<VkVertexInputAttributeDescription, 2> get_attribute_descriptions()
		{
			std::array<VkVertexInputAttributeDescription, 2> attribute_description = {};
			attribute_description[0].binding = 1;
			attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_description[0].location = 3;
			attribute_description[0].offset = offsetof(vertex_ext, tangent);

			attribute_description[1].binding = 1;
			attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_description[1].location = 4;
			attribute_description[1].offset = offsetof(vertex_ext, bitangent);

			return attribute_description;
		}
	};

	constexpr decltype(auto) get_vertex_input_binding_descriptions()
	{
		auto vertex_binding = vertex::get_binding_description();
		auto vertex_ext_binding = vertex_ext::get_binding_description();
		return std::array<VkVertexInputBindingDescription, 2>{vertex_binding, vertex_ext_binding};
	}

	constexpr decltype(auto) get_vertex_input_attribute_descriptions()
	{
		auto vertex_attribs = vertex::get_attribute_descriptions();
		auto vertex_ext_attribs = vertex_ext::get_attribute_descriptions();
		std::array<VkVertexInputAttributeDescription, vertex_attribs.size() + vertex_ext_attribs.size()> attribs = {};
		for (size_t i = 0; i < vertex_attribs.size(); ++i)
			attribs[i] = vertex_attribs[i];
		for (size_t i = 0; i < vertex_ext_attribs.size(); ++i)
			attribs[i + vertex_attribs.size()] = vertex_ext_attribs[i];
		return attribs;
	}

	

	//unique id generator
	template<typename T>
	inline unique_id get_unique_id()
	{
		static unique_id next_id = 0;
		return next_id++;
	}


	enum class dealloc_pattern
	{
		destruct,
		ignore
	};
	template<typename T, dealloc_pattern dealloc=dealloc_pattern::destruct>
	class slot_map
	{
	public:
		struct slot
		{
			uint32_t index;
			uint32_t generation;
		};

		slot_map() {}
		slot_map(const slot_map&) = delete;

		slot_map(slot_map&& other) :
			m_chunks(other.m_chunks),
			m_chunks_size(other.m_chunks_size),
			m_chunks_capacity(other.m_chunks_capacity),
			m_size(other.m_size),
			m_capacity(other.m_capacity),
			m_next(other.m_next)
		{
			other.m_chunks = nullptr;
			other.m_chunks_size = 0;
			other.m_chunks_capacity = 0;
			other.m_size = 0;
			other.m_capacity = 0;
		}

		slot_map& operator=(slot_map&& other)
		{
			if (this == *other)
				return;

			if (m_chunks != nullptr)
			{
				for_each([](T& t) { t.~T(); });

				for (uint32_t i = 0; i < m_chunks_size; ++i)
					std::free(m_chunks[i].data);

				std::free(m_chunks);
			}

			m_chunks = other.m_chunks;
			m_chunks_size = other.m_chunks;
			m_chunks_capacity = other.m_chunks_capacity;
			m_size = other.m_size;
			m_capacity = other.m_capacity;
			m_next = other.m_next;

			other.m_chunks = nullptr;
			m_chunks_size = 0;
			m_chunks_capacity = 0;
			m_size = 0;
			m_capacity = 0;
		}

		~slot_map()
		{
			if (m_chunks == nullptr)
				return;

			if constexpr(dealloc == dealloc_pattern::destruct)
			{
				for_each([](T& t) { t.~T(); });
			}

			for (uint32_t i = 0; i < m_chunks_size; ++i)
				std::free(m_chunks[i].data);

			std::free(m_chunks);
		}

		void swap(slot_map& other)
		{
			if (this == *other)
				return;

			auto chunks = m_chunks;
			auto chunks_size = m_chunks_size;
			auto chunks_capacity = m_chunks_capacity;
			auto size = m_size;
			auto capacity = m_capacity;
			auto next = m_next;

			m_chunks = other.m_chunks;
			m_chunks_size = other.m_chunks;
			m_chunks_capacity = other.m_chunks_capacity;
			m_size = other.m_size;
			m_capacity = other.m_capacity;
			m_next = other.m_next;

			other.m_chunks = chunks;
			other.m_chunks_size = chunks_size;
			other.m_chunks_capacity = capacity;
			other.m_size = size;
			other.m_capacity = capacity;
			other.m_next = next;
		}

		void reset()
		{
			if (m_chunks == nullptr)
				return;

			if constexpr(dealloc == dealloc_pattern::destruct)
			{
				for_each([](T& t) { t.~T(); });
			}


			for (uint32_t i = 0; i < m_chunks_size; ++i)
				std::free(m_chunks[i].data);

			std::free(m_chunks);

			m_chunks = nullptr;
			m_chunks_size = 0;
			m_chunks_capacity = 0;
			m_size = 0;
			m_capacity = 0;
		}

		template<typename... Ts>
		slot emplace(Ts&&... args)
		{
			if (m_size == m_capacity)
				extend();

			slot* next = get_slot(m_next);

			slot ret = { m_next, next->generation };

			new(get_data(m_size)) T(std::forward<Ts>(args)...);
			*(get_slot_view(m_size)) = next;

			m_next = next->index;
			next->index = m_size;
			++m_size;

			return ret;
		}

		template<typename... Ts>
		bool replace(slot& id, Ts&&... args)
		{
			slot* real_id = get_slot(id.index);
			if (real_id->generation != id.generation)
				return false;

			T* data = get_data(real_id->index);

			if constexpr (dealloc==dealloc_pattern::destruct)
				data->~T();

			new(data) T(std::forward<Ts>(args)...);
			++real_id->generation;
			++id.generation;
			return true;
		}



		T* get(const slot& id)
		{
			slot* real_id = get_slot(id.index);
			if (real_id->generation != id.generation)
				return nullptr;

			return get_data(real_id->index);
		}

		bool destroy(const slot& id)
		{
			slot* real_id = get_slot(id.index);
			if (real_id->generation != id.generation)
				return false;

			T* deleted = get_data(real_id->index);
			++real_id->generation;

			if constexpr(dealloc==dealloc_pattern::destruct)
				deleted->~T();

			--m_size;

			if (real_id->index != m_size)
			{
				*deleted = std::move(*get_data(m_size));
				get_slot(m_size)->index = real_id->index;
			}

			real_id->index = m_next;
			m_next = id.index;
			return true;
		}

		template<typename Callable>
		void for_each(Callable&& f)
		{
			uint32_t remaining = m_size;
			auto current_chunk = m_chunks;
			uint32_t counter = 0;
			while (remaining>0)
			{
				auto data = current_chunk->data;
				uint32_t steps = remaining > chunk_size ? chunk_size : remaining;
				remaining -= steps;
				while (steps-- > 0)
					f(*data++);
			}
		}

		void print()
		{
			std::cout << "slot map content:\n" <<
				'\t' << "size: " << m_size << '\n' <<
				'\t' << "capacity: " << m_capacity << '\n';
			for (uint32_t i = 0; i < m_chunks_size; ++i)
			{
				std::cout << '\t' << "chunk" << i << '\n';
				for (uint32_t j = 0; j < chunk_size; ++j)
				{
					std::cout << "\t\t" << "slot:\n" <<
						"\t\t\t" << "index: " << (m_chunks[i].slots + j)->index << '\n' <<
						"\t\t\t" << "generation: " << (m_chunks[i].slots + j)->generation << '\n';

					std::cout << "\t\t" << "data: " << m_chunks[i].data[j] << '\n';
				}
			}
		}

	private:
		static const uint32_t chunk_size_bit_count = 8;
		static const uint32_t chunk_size = 256;
		static const uint32_t chunk_index_mask = chunk_size - 1;

		struct chunk
		{
			T* data;
			slot** slot_views;
			slot* slots;
		};

		uint32_t m_size = 0;
		uint32_t m_capacity = 0;
		uint32_t m_next;

		chunk* m_chunks = nullptr;
		uint32_t m_chunks_size = 0;
		uint32_t m_chunks_capacity = 0;

		void extend()
		{
			if (m_chunks_size == m_chunks_capacity)
			{
				m_chunks_capacity = m_chunks_capacity == 0 ? 1 : 2 * m_chunks_capacity;
				m_chunks = reinterpret_cast<chunk*>(std::realloc(m_chunks, sizeof(chunk)*m_chunks_capacity));
			}

			char* memory = reinterpret_cast<char*>(std::malloc((sizeof(T) + sizeof(slot*) + sizeof(slot))*chunk_size));
			chunk new_chunk = {
				reinterpret_cast<T*>(memory),
				reinterpret_cast<slot**>(memory + sizeof(T)*chunk_size),
				reinterpret_cast<slot*>(memory + (sizeof(slot*) + sizeof(T))*chunk_size)
			};

			m_chunks[m_chunks_size] = new_chunk;
			++m_chunks_size;

			m_next = m_size;

			slot* id = new_chunk.slots;

			uint32_t i = m_capacity + 1;
			m_capacity += chunk_size;
			while (i < m_capacity)
			{
				id->index = i++;
				++id;
			}
		}

		inline T* get_data(uint32_t index)
		{
			return m_chunks[index >> chunk_size_bit_count].data + (index&chunk_index_mask);
		}

		inline slot* get_slot(uint32_t index)
		{
			auto i0 = index >> chunk_size_bit_count;
			auto i1 = index&chunk_size;
			return m_chunks[index >> chunk_size_bit_count].slots + (index&chunk_index_mask);
		}

		inline slot** get_slot_view(uint32_t index)
		{
			return m_chunks[index >> chunk_size_bit_count].slot_views + (index&chunk_index_mask);
		}
	};

	class timer
	{
	public:
		void start()
		{
			time = std::chrono::high_resolution_clock::now();
		}
		void stop()
		{
			auto end_time = std::chrono::high_resolution_clock::now();
			duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - time).count() / 1000000.f;
		}
		float get()
		{
			return duration;
		}
		void wait_until(std::chrono::milliseconds d)
		{
			duration = d.count() / 1000.f;
			std::this_thread::sleep_until(time + d);
			
		}
	private:
		std::chrono::time_point<std::chrono::steady_clock> time;
		float duration;
	};
}

namespace std
{
	template<> struct hash<rcq::vertex>
	{
		size_t operator()(const rcq::vertex& v) const
		{
			return ((hash<glm::vec3>()(v.pos) ^
				(hash<glm::vec3>()(v.normal) << 1)) >> 1) ^
				(hash<glm::vec2>()(v.tex_coord) << 1);
		}
	};
}