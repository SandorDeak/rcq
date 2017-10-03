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
#include <variant>


namespace rcq
{
	//interface related
	enum LIGHT_FLAG
	{
		LIGHT_FLAG_SHADOW_MAP
	};

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

	enum LIGHT_TYPE
	{
		LIGHT_TYPE_OMNI,
		//LIGHT_TYPE_SPOT,
		//LIGHT_TYPE_DIR,
		LIGHT_TYPE_COUNT
	};

	enum RENDER_PASS
	{
		RENDER_PASS_OMNI_LIGHT_SHADOW,
		RENDER_PASS_BASIC,
		RENDER_PASS_COUNT
	};

	enum RESOURCE_TYPE
	{
		RESOURCE_TYPE_MAT,
		RESOURCE_TYPE_MESH,
		RESOURCE_TYPE_TR,
		RESOURCE_TYPE_LIGHT,

		//for render passes
		RESOURCE_TYPE_MEMORY,

		RESOURCE_TYPE_COUNT
	};

	enum SAMPLER_TYPE
	{
		SAMPLER_TYPE_SIMPLE,
		SAMPLER_TYPE_OMNI_LIGHT_SHADOW_MAP,
		SAMPLER_TYPE_COUNT
	};

	enum DESCRIPTOR_SET_LAYOUT_TYPE
	{
		DESCRIPTOR_SET_LAYOUT_TYPE_MAT,
		DESCRIPTOR_SET_LAYOUT_TYPE_TR,
		DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT,
		DESCRIPTOR_SET_LAYOUT_TYPE_COUNT
	};

	struct material_data;
	struct transform_data;
	struct camera_data;
	struct omni_light_data;
	struct spotlight_data;
	struct dir_light_data;

	

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

	typedef std::variant<omni_light_data, spotlight_data, dir_light_data> light_data;
	typedef std::tuple<unique_id, light_data, USAGE, bool> build_light_info; 
	enum
	{
		BUILD_LIGHT_INFO_ID,
		BUILD_LIGHT_INFO_DATA,
		BUILD_LIGHT_INFO_USAGE,
		BUILD_LIGHT_INFO_MAKE_SHADOW_MAP 
	};  


	typedef std::tuple<unique_id, std::vector<VkMemoryAllocateInfo>> build_memory_info;

	typedef std::tuple<unique_id, unique_id, unique_id, unique_id, LIFE_EXPECTANCY> build_renderable_info;
	enum
	{
		BUILD_RENDERABLE_INFO_RENDERABLE_ID,
		BUILD_RENDERABLE_INFO_TR_ID,
		BUILD_RENDERABLE_INFO_MESH_ID,
		BUILD_RENDERABLE_INFO_MAT_ID,
		BUILD_RENDERABLE_INFO_LIFE_EXPECTANCY
	};

	typedef std::tuple<unique_id, unique_id, LIFE_EXPECTANCY> build_light_renderable_info;
	enum
	{
		BUILD_LIGHT_RENDERABLE_INFO_ID,
		BUILD_LIGHT_RENDERABLE_INFO_RES_ID,
		BUILD_LIGHT_RENDERABLE_INFO_LIFE_EXPECTANCY
	};

	typedef std::tuple<unique_id, transform_data> update_tr_info;
	enum
	{
		UPDATE_TR_INFO_TR_ID,
		UPDATE_TR_INFO_TR_DATA
	};


	struct material_data
	{
		glm::vec3 color;
		float roughness;
		float metal;
		float height_scale;
		uint32_t flags;
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

	extern const size_t DISASSEMBLER_COMMAND_QUEUE_MAX_SIZE;
	extern const size_t CORE_COMMAND_QUEUE_MAX_SIZE;
	extern const size_t SHADOW_MAP_SIZE;

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
		SAMPLER_TYPE sampler_type;
	};

	typedef std::array<texture, TEX_TYPE_COUNT> mat_texs;
	typedef std::pair<uint32_t, size_t> cell_info; //block id, offset

	struct material
	{
		mat_texs texs;
		MAT_TYPE type;
		VkDescriptorSet ds;
		uint32_t pool_index;
		VkBuffer data;
		cell_info cell;
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

	struct light
	{
		VkDescriptorSet ds;
		uint32_t pool_index;
		USAGE usage;
		VkBuffer buffer;
		void* data;
		cell_info cell;
		texture shadow_map;
		VkFramebuffer shadow_map_fb;
		LIGHT_TYPE type;
	};


	struct renderable
	{
		VkBuffer vb;
		VkBuffer veb;
		VkBuffer ib;
		uint32_t size;
		VkDescriptorSet tr_ds;
		VkDescriptorSet mat_ds;
		uint32_t type;
		bool destroy;
		unique_id id;
	};

	struct light_renderable
	{
		VkDescriptorSet ds;
		texture shadow_map;
		uint32_t type;
		bool destroy;
		VkFramebuffer shadow_map_fb;
		unique_id id;
	};

	typedef std::array<std::vector<renderable>, MAT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> renderable_container;
	typedef std::array < std::vector<light_renderable>, LIGHT_TYPE_COUNT*LIFE_EXPECTANCY_COUNT> light_renderable_container;

	struct core_package
	{
		std::vector<build_renderable_info> build_renderable;
		std::vector<build_light_renderable_info> build_light_renderable;
		std::vector<unique_id> destroy_renderable;
		std::vector<unique_id> destroy_light_renderable;
		std::vector<update_tr_info> update_tr;
		std::optional<camera_data> update_cam;
		std::optional<std::promise<void>> confirm_destroy;
		bool render = false;
	};


	typedef std::vector<VkDeviceMemory> memory;

	typedef std::tuple<std::vector<build_mat_info>, std::vector<build_mesh_info>, std::vector<build_tr_info>, 
		std::vector<build_light_info>, std::vector<build_memory_info>> build_package;

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
	typedef std::packaged_task<light()> build_light_task;
	typedef std::packaged_task<memory()> build_memory_task;

	typedef std::tuple<std::vector<build_mat_task>, std::vector<build_mesh_task>, std::vector<build_tr_task>, 
		std::vector<build_light_task>, std::vector<build_memory_task>> build_task_package;


	template<size_t res_type>
	struct resource_typename;

	template<> struct resource_typename<RESOURCE_TYPE_MAT> { typedef material type; };
	template<> struct resource_typename<RESOURCE_TYPE_MESH> { typedef mesh type; };
	template<> struct resource_typename<RESOURCE_TYPE_TR>{ typedef transform type; };
	template<> struct resource_typename<RESOURCE_TYPE_MEMORY> { typedef memory type; };
	template<> struct resource_typename<RESOURCE_TYPE_LIGHT> { typedef light type; };

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

	template<> struct render_pass_typename<RENDER_PASS_BASIC> { typedef  basic_pass type; };
	template<> struct render_pass_typename<RENDER_PASS_OMNI_LIGHT_SHADOW> { typedef omni_light_shadow_pass type; };

	struct omni_light_data
	{
		glm::vec3 pos;
		uint32_t flags;
		glm::vec3 radiance;
	};

	struct spotlight_data
	{
		glm::vec3 pos;
		float angle;
		glm::vec3 dir;
		float penumbra_angle;
		glm::vec3 radiance;
		float umbra_angle;
	};

	struct dir_light_data
	{
		glm::vec3 dir;
		uint32_t padding0;
		glm::vec3 irradiance;
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