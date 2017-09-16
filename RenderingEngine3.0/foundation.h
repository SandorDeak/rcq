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


namespace rcq
{
	//interface related

	enum TEX_TYPE
	{
		TEX_TYPE_COLOR,
		TEX_TYPE_ROUGHNESS,
		TEX_TYPE_METAL,
		TEX_TYPE_NORMAL,
		TEY_TYPE_HEIGHT,
		TEX_TYPE_AO,
		TEX_TYPE_COUNT
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

	enum MAT_TYPE
	{
		/*MAT_TYPE_OPAQUE,
		MAT_TYPE_REFLECTIVE_EM,
		MAT_TYPE_REFRACTIVE_EM*/
		MAT_TYPE_BASIC,
		MAT_TYPE_COUNT
		//and more and more e.g. water, fire...
	};

	enum RENDER_PASS
	{
		RENDER_PASS_BASIC,
		RENDER_PASS_COUNT
	};

	enum RESOURCE_TYPE
	{
		RESOURCE_TYPE_MAT,
		RESOURCE_TYPE_MESH,
		RESOURCE_TYPE_TR,

		//for render passes
		RESOURCE_TYPE_MEMORY,

		RESOURCE_TYPE_COUNT
	};

	struct material_data;
	struct transform_data;
	struct camera_data;

	

	typedef std::array<std::string, TEX_TYPE_COUNT> texfiles;
	typedef size_t unique_id;


	typedef std::tuple<unique_id, material_data, texfiles, MAT_TYPE> build_mat_info;
	enum 
	{
		BUILD_MAT_INFO_MAT_ID,
		BUILD_MAT_INFO_MAT_DATA,
		BUILD_MAT_INFO_TEXINFOS,
		BUILD_MAT_INFO_MAT_TYPE
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

	typedef std::tuple<unique_id, std::vector<VkMemoryRequirements>> build_memory_info;

	typedef std::tuple<unique_id, unique_id, unique_id, unique_id, LIFE_EXPECTANCY> build_renderable_info;
	enum
	{
		BUILD_RENDERABLE_INFO_RENDERABLE_ID,
		BUILD_RENDERABLE_INFO_TR_ID,
		BUILD_RENDERABLE_INFO_MESH_ID,
		BUILD_RENDERABLE_INFO_MAT_ID,
		BUILD_RENDERABLE_INFO_LIFE_EXPECTANCY
	};

	typedef std::tuple<unique_id, transform_data> update_tr_info;
	enum
	{
		UPDATE_TR_INFO_TR_ID,
		UPDATE_TR_INFO_TR_DATA
	};


	struct material_data
	{
		uint32_t flags;
		float metal;
		float roughness;
		float refraction_index;
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
	std::vector<char> read_file(const std::string& filename);

	VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code);

	VkFormat find_support_format(VkPhysicalDevice device, const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features);

	uint32_t find_memory_type(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties);

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


	extern const VkAllocationCallbacks* host_memory_manager;

	extern const size_t DISASSEMBLER_COMMAND_QUEUE_MAX_SIZE;
	extern const size_t CORE_COMMAND_QUEUE_MAX_SIZE;

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
	};

	struct texture
	{
		VkImage image;
		VkImageView view;
		VkDeviceMemory memory;
	};

	typedef std::array<texture, TEX_TYPE_COUNT> mat_texs;

	struct material
	{
		mat_texs texs;
		MAT_TYPE type;
		VkDescriptorSet ds;
		VkBuffer data;
	};


	struct mesh
	{
		VkBuffer vb; //vertex
		VkBuffer ib; //index
		VkBuffer veb; //vertex ext
		VkDeviceMemory memory;
		VkDeviceSize size;
	};

	typedef std::pair<uint32_t, size_t> cell_info; //block id, offset


	struct transform
	{
		VkDescriptorSet ds;
		USAGE usage;
		VkBuffer buffer;
		char** data;
		cell_info cell;
	};

	struct renderable
	{
		VkDescriptorSet mat_ds;	
		VkBuffer vb;
		VkBuffer veb;
		VkBuffer ib;
		VkDeviceSize size;
		VkDescriptorSet tr_ds;
		uint32_t type;
		bool destroy;
	};

	struct core_package
	{
		std::vector<build_renderable_info> build_renderable;
		std::vector<unique_id> destroy_renderable;
		std::vector<update_tr_info> update_tr;
		std::optional<camera_data> update_cam;
		std::optional<std::promise<void>> confirm_destroy;
		bool render = false;
	};


	typedef std::vector<VkDeviceMemory> memory;

	typedef std::tuple<std::vector<build_mat_info>, std::vector<build_mesh_info>, std::vector<build_tr_info>, 
		std::vector<build_memory_info>> build_package;

	typedef std::array<std::vector<unique_id>, RESOURCE_TYPE_COUNT> destroy_ids;

	struct destroy_package
	{
		destroy_ids ids;
		std::optional<std::future<void>> destroy_confirmation;
	};

	struct command_package
	{
		std::optional<core_package> core_p;
		std::optional<build_package> resource_manager_build_p;
		std::optional<destroy_package> resource_mananger_destroy_p;
	};

	typedef std::packaged_task<mesh()> build_mesh_task;
	typedef std::packaged_task<material()> build_mat_task;
	typedef std::packaged_task<transform()> build_tr_task;

	typedef std::packaged_task<memory()> build_memory_task;

	typedef std::tuple<std::vector<build_mat_task>, std::vector<build_mesh_task>, std::vector<build_tr_task>,
		std::vector<build_memory_task>> build_task_package;


	template<size_t res_type>
	struct resource_typename { typedef int type; };

	template<> struct resource_typename<RESOURCE_TYPE_MAT> { typedef material type; };
	template<> struct resource_typename<RESOURCE_TYPE_MESH> { typedef mesh type; };
	template<> struct resource_typename<RESOURCE_TYPE_TR>{ typedef transform type; };
	template<> struct resource_typename<RESOURCE_TYPE_MEMORY> { typedef memory type; };
	

	class basic_pass;
	template<size_t res_type>
	struct render_pass_typename {};

	template<> struct render_pass_typename<RENDER_PASS_BASIC> { typedef  basic_pass type; };


	struct render_target
	{
		VkFramebuffer framebuffer;
		texture tex;
	};

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

	constexpr decltype(auto) get_vetex_input_attribute_descriptions()
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

	
}
