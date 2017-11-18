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
#include <fstream>

#define PI 3.1415926535897f


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
		RENDERABLE_TYPE_SKY,
		RENDERABLE_TYPE_TERRAIN,
		RENDERABLE_TYPE_WATER,
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
		RESOURCE_TYPE_SKY,
		RESOURCE_TYPE_TERRAIN,
		RESOURCE_TYPE_WATER,
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
		DESCRIPTOR_SET_LAYOUT_TYPE_SKY,
		DESCRIPTOR_SET_LAYOUT_TYPE_TR,
		DESCRIPTOR_SET_LAYOUT_TYPE_LIGHT_OMNI,
		DESCRIPTOR_SET_LAYOUT_TYPE_SKYBOX,
		DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN,
		DESCRIPTOR_SET_LAYOUT_TYPE_TERRAIN_COMPUTE,
		DESCRIPTOR_SET_LAYOUT_TYPE_WATER_COMPUTE,
		DESCRIPTOR_SET_LAYOUT_TYPE_WATER,
		DESCRIPTOR_SET_LAYOUT_TYPE_COUNT
	};

	enum class RESOURCE_GET
	{
		IF_READY,
		IMMEDIATELY
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

	typedef std::tuple<unique_id, std::string, glm::uvec3, glm::uvec2> build_sky_info;
	enum
	{
		BUILD_SKY_INFO_ID,
		BUILD_SKY_INFO_FILENAME,
		BUILD_SKY_INFO_SKY_IMAGE_SIZE,
		BUILD_SKY_INFO_TRANSMITTANCE_SIZE
	};

	typedef std::tuple<unique_id, std::string, uint32_t, glm::uvec2, glm::vec3, glm::uvec2> build_terrain_info;
	enum
	{
		BUILD_TERRAIN_INFO_ID,
		BUILD_TERRAIN_INFO_FILENAME,
		BUILD_TERRAIN_INFO_MIP_LEVEL_COUNT,
		BUILD_TERRAIN_INFO_LEVEL0_IMAGE_SIZE,
		BUILD_TERRAIN_INFO_SIZE_IN_METERS,
		BUILD_TERRAIN_INFO_LEVEL0_TILE_SIZE
	};

	typedef std::tuple<unique_id, std::string, glm::vec2, float, float> build_water_info;
	enum
	{
		BUILD_WATER_INFO_ID,
		BUILD_WATER_INFO_FILENAME,
		BUILD_WATER_INFO_GRID_SIZE_IN_METERS,
		BUILD_WATER_INFO_BASE_FREQUENCY,
		BUILD_WATER_INFO_A
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

	const uint32_t MAX_REQUEST_COUNT = 256;
	const uint32_t MAX_TILE_COUNT = 128;
	const uint32_t MAX_TILE_COUNT_LOG2 = 7;

	struct terrain_data
	{
		glm::vec2 terrain_size_in_meters;
		glm::vec2 meter_per_tile_size_length;
		glm::ivec2 tile_count;
		float mip_level_count;
		float height_scale;
	};

	struct terrain_request_data
	{
		glm::vec2 tile_size_in_meter;
		float mip_level_count;
		uint32_t request_count;
		uint32_t requests[MAX_REQUEST_COUNT];
	};


	//engine related

	extern const uint32_t GRID_SIZE;


	std::vector<char> read_file(const std::string_view& filename);

	VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code);
	VkPipelineLayout create_layout(VkDevice device, const std::vector<VkDescriptorSetLayout>& dsls, const std::vector<VkPushConstantRange>& push_constants, const VkAllocationCallbacks* alloc);
	void create_shaders(VkDevice device, const std::vector<std::string_view>& files, const std::vector<VkShaderStageFlagBits>& stages,
		VkPipelineShaderStageCreateInfo* shaders);

	VkFormat find_support_format(VkPhysicalDevice device, const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features);

	uint32_t find_memory_type(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties);

	struct base_info;
	void create_staging_buffer(const base_info& base, VkDeviceSize size, VkBuffer & buffer, VkDeviceMemory & memory,
		const VkAllocationCallbacks* alloc);

	VkCommandBuffer begin_single_time_command(VkDevice device, VkCommandPool command_pool);
	void end_single_time_command_buffer(VkDevice device, VkCommandPool cp, VkQueue queue_for_submit, VkCommandBuffer cb);


	inline VkFormat find_depth_format(VkPhysicalDevice device)
	{
		return find_support_format(device, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	inline glm::vec3 get_orthonormal(const glm::vec3& v)
	{
		if (fabsf(v.x) > 0.001f || fabsf(v.y)>0.001f)
		{
			return glm::normalize(glm::vec3(-v.y, v.x, 0.f));
		}
		else
		{
			return glm::normalize(glm::vec3(-v.z, 0.f, v.x));
		}
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

	template<typename T>
	constexpr T componentwise_max(const T& v, const T& w)
	{
		T ret = {};
		for (int i = 0; i < v.length(); ++i)
		{
			ret[i] = v[i] < w[i] ? w[i] : v[i];
		}
		return ret;
	}

	template<typename T>
	constexpr T componentwise_min(const T& v, const T& w)
	{
		T ret = {};
		for (int i = 0; i < v.length(); ++i)
		{
			ret[i] = v[i] < w[i] ? v[i] : w[i];
		}
		return ret;
	}

	typedef uint32_t pool_id;

	class device_memory_pool
	{
	public:
		struct cell_info
		{
			size_t chunk_id;
			size_t cell_id;
		};
		device_memory_pool()
		{
			m_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		}

		void set_alloc_info(VkDevice device, const VkAllocationCallbacks* alloc, uint32_t memory_type_index,
			size_t chunk_size, size_t sizeof_cell, bool map)
		{
			m_device = device;
			m_alloc = alloc;
			m_alloc_info.memoryTypeIndex = memory_type_index;
			m_alloc_info.allocationSize = chunk_size*sizeof_cell;
			m_chunk_size = chunk_size;
			m_sizeof_cell = sizeof_cell;
			m_map = map;
		}

		void free()
		{
			for (auto c : m_chunks)
				vkFreeMemory(m_device, c, m_alloc);
		}

		cell_info pop_cell()
		{
			if (!m_cell_pool.empty())
			{
				auto ret = m_cell_pool.top();
				m_cell_pool.pop();
				return ret;
			}
			VkDeviceMemory chunk;
			if (vkAllocateMemory(m_device, &m_alloc_info, m_alloc, &chunk) != VK_SUCCESS)
				throw std::runtime_error("failed to allocate memory!");
			for (size_t i = 1; i < m_chunk_size; ++i)
				m_cell_pool.push({ m_chunks.size(), i });
			m_chunks.push_back(chunk);
			if (m_map)
			{
				void* raw_data;
				vkMapMemory(m_device, chunk, 0, m_alloc_info.allocationSize, 0, &raw_data);
				m_mapped_chunks.push_back(reinterpret_cast<char*>(raw_data));
			}
			return { m_chunks.size() - 1, 0 };
		}
		void push_cell(const cell_info& c)
		{
			m_cell_pool.push(c);
		}
		size_t get_offset(const cell_info& c)
		{
			return c.cell_id*m_sizeof_cell;
		}
		VkDeviceMemory get_memory(const cell_info& c)
		{
			return m_chunks[c.chunk_id];
		}
		char* get_pointer(const cell_info& c)
		{
			return m_mapped_chunks[c.chunk_id] + c.cell_id*m_sizeof_cell;
		}
	private:
		std::vector<VkDeviceMemory> m_chunks;
		std::vector<char*> m_mapped_chunks;
		bool m_map;
		VkMemoryAllocateInfo m_alloc_info;
		size_t m_chunk_size;
		size_t m_sizeof_cell;
		std::stack<cell_info> m_cell_pool;
		const VkAllocationCallbacks* m_alloc;
		VkDevice m_device;
	};

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
		int compute_family = -1;

		bool complete()
		{
			return graphics_family >= 0 && present_family >= 0 && compute_family>=0;
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
		VkQueue compute_queue;
		GLFWwindow* window;
		VkFormatProperties format_properties;

		mutable std::mutex graphics_queue_mutex;
		mutable std::mutex compute_queue_mutex;
	};

	struct texture
	{
		VkImage image;
		VkImageView view;
		VkDeviceMemory memory;
		VkSampler sampler;
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

	struct sky
	{
		VkDescriptorSet ds;
		std::array<texture, 3> tex;
		uint32_t pool_index;
	};


	struct virtual_texture
	{
		VkImage image;
		VkImageView view;
		VkSampler sampler;
		VkDeviceMemory dummy_page_and_mip_tail;
		//std::vector<std::vector<tile>> tiles; //should contain the terrain_tile-s!!!!!!!!!!!!!
		//glm::uvec2 image_size;
		glm::uvec2 page_size;
		size_t page_size_in_bytes;
		size_t mip_tail_size;
		size_t mip_tail_offset;

		//tile& get_tile(const glm::uvec2& tile_id) { return tiles[tile_id.x][tile_id.y]; }
	};

	struct terrain
	{
		virtual_texture tex;
		glm::uvec2 tile_count;
		std::vector<glm::uvec2> tile_size_in_pages;
		std::ifstream* files;
		size_t mip_level_count;
		VkDescriptorSet ds;
		VkDescriptorSet request_ds;
		device_memory_pool page_pool;

		VkBuffer staging_buffer;
		VkDeviceMemory staging_buffer_memory;
		char* staging_buffer_data;

		VkBuffer data_buffer;
		VkDeviceMemory data_buffer_memory;
		terrain_data* data;

		VkBuffer request_data_buffer;
		VkDeviceMemory request_buffer_memory;
		terrain_request_data* request_data;

		VkBufferView requested_mip_levels_view;
		VkBuffer requested_mip_levels_buffer;
		VkDeviceMemory requested_mip_levels_memory;
		float* requested_mip_levels_data;

		VkBufferView current_mip_levels_view;
		VkBuffer current_mip_levels_buffer;
		VkDeviceMemory current_mip_levels_memory;
		float* current_mip_levels_data;

		pool_id pool_index;

	};

	struct water
	{
		VkDescriptorSet ds;
		VkDescriptorSet generator_ds;

		texture noise;
		texture tex;

		VkBuffer fft_params;
		VkDeviceMemory fft_params_mem;

		glm::vec2 grid_size_in_meters;

		pool_id pool_index;

		struct fft_params_data
		{
			glm::vec2 two_pi_per_L; //l=grid side length in meters
			float sqrtA;
			float base_frequency;
		};
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

		glm::vec2 wind;
		float time;
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

		//for terrain
		glm::uvec2 tiles_count;
		VkDescriptorSet request_ds;
		std::vector<VkDescriptorSet> mat_dss;


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
		std::vector<build_sky_info>,
		std::vector<build_terrain_info>,
		std::vector<build_water_info>,
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

	typedef std::packaged_task<material_opaque()> build_mat_opaque_task;
	typedef std::packaged_task<material_em()> build_mat_em_task;
	typedef std::packaged_task<sky()> build_sky_task;
	typedef std::packaged_task<terrain()> build_terrain_task;
	typedef std::packaged_task<water()> build_water_task;
	typedef std::packaged_task<light_omni()> build_light_omni_task;
	typedef std::packaged_task<skybox()> build_skybox_task;
	typedef std::packaged_task<mesh()> build_mesh_task;
	typedef std::packaged_task<transform()> build_tr_task;
	typedef std::packaged_task<memory()> build_memory_task;

	typedef std::tuple<
		std::vector<build_mat_opaque_task>,
		std::vector<build_mat_em_task>,
		std::vector<build_sky_task>,
		std::vector<build_terrain_task>,
		std::vector<build_water_task>,
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
	template<> struct resource_typename<RESOURCE_TYPE_SKY> { typedef sky type; };
	template<> struct resource_typename<RESOURCE_TYPE_MESH> { typedef mesh type; };
	template<> struct resource_typename<RESOURCE_TYPE_TR>{ typedef transform type; };
	template<> struct resource_typename<RESOURCE_TYPE_MEMORY> { typedef memory type; };
	template<> struct resource_typename<RESOURCE_TYPE_LIGHT_OMNI> { typedef light_omni type; };
	template<> struct resource_typename<RESOURCE_TYPE_SKYBOX> { typedef skybox type; };
	template<> struct resource_typename<RESOURCE_TYPE_TERRAIN> { typedef terrain type; };
	template<> struct resource_typename<RESOURCE_TYPE_WATER> { typedef water type; };


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

	class allocator
	{
	public:
		allocator(std::string info) : m_info(std::move(info)) 
		{
			m_callbacks.pUserData = reinterpret_cast<void*>(this);
			m_callbacks.pfnAllocation = &alloc_static;
			m_callbacks.pfnFree = &free_static;
			m_callbacks.pfnReallocation = &realloc_static;
			m_callbacks.pfnInternalAllocation = &internal_alloc_notification_static;
			m_callbacks.pfnInternalFree = &internal_free_notification_static;
		}

		operator const VkAllocationCallbacks*()
		{
			return &m_callbacks;
		}
	private:
		std::string m_info;
		VkAllocationCallbacks m_callbacks;
		void* alloc(size_t size, size_t alignment)
		{
			//std::cout << m_info << " allocate: " << size << ' ' << alignment << std::endl;

			return _aligned_malloc(size, alignment);
		}

		void free(void* ptr)
		{
			//std::cout << m_info << " free " << std::endl;
			_aligned_free(ptr);
		}

		void* realloc(void* original, size_t size, size_t alignment, VkSystemAllocationScope scope)
		{
			//std::cout << m_info << " reallocate: " << size << ' ' << alignment << std::endl;

			return _aligned_realloc(original, size, alignment);
		}

		void internal_alloc_notification(size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope)
		{
			//std::cout << m_info << "internal alloc: " << size << std::endl;
		}

		void internal_free_notification(size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope)
		{
			//std::cout << m_info << "internal free: " << size << std::endl;
		}


		static void* VKAPI_CALL alloc_static(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope alloc_scope)
		{

			return static_cast<allocator*>(user_data)->alloc(size, alignment);
		}

		static void VKAPI_CALL free_static(void* user_data, void* ptr)
		{
			static_cast<allocator*>(user_data)->free(ptr);
		}

		static void* VKAPI_CALL realloc_static(void* user_data, void* original, size_t size, size_t alignment, VkSystemAllocationScope alloc_scope)
		{
			return static_cast<allocator*>(user_data)->realloc(original, size, alignment, alloc_scope);
		}

		static void VKAPI_CALL internal_alloc_notification_static(void* user_data, size_t size, VkInternalAllocationType type,
			VkSystemAllocationScope scope)
		{
			static_cast<allocator*>(user_data)->internal_alloc_notification(size, type, scope);
		}

		static void VKAPI_CALL internal_free_notification_static(void* user_data, size_t size, VkInternalAllocationType type,
			VkSystemAllocationScope scope)
		{
			static_cast<allocator*>(user_data)->internal_free_notification(size, type, scope);
		}
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