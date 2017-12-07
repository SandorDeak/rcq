#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_SWIZZLE
#include<glm\glm.hpp>
#include <glm\gtx\hash.hpp>
#include <glm\gtx\transform.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>

//#include "slot_map.h"

#include <atomic>

#include "list.h"
#include "stack.h"
#include "array.h"
#include "pool_memory_resource.h"

std::atomic_bool a;

namespace rcq
{
	const uint32_t SWAP_CHAIN_SIZE = 2;

	enum RES_TYPE : uint32_t
	{
		RES_TYPE_MAT_OPAQUE,
		//RES_TYPE_MAT_EM,
		RES_TYPE_SKY,
		RES_TYPE_TERRAIN,
		RES_TYPE_WATER,
		RES_TYPE_MESH,
		RES_TYPE_TR,
		RES_TYPE_COUNT
	};

	enum REND_TYPE : uint32_t
	{
		REND_TYPE_OPAQUE_OBJECT,
		REND_TYPE_SKY,
		REND_TYPE_TERRAIN,
		REND_TYPE_WATER,
		REND_TYPE_COUNT
	};

	enum TEX_TYPE : uint32_t
	{
		TEX_TYPE_COLOR,
		TEX_TYPE_ROUGHNESS,
		TEX_TYPE_METAL,
		TEX_TYPE_NORMAL,
		TEX_TYPE_HEIGHT,
		TEX_TYPE_AO,
		TEX_TYPE_COUNT
	};

	enum DSL_TYPE
	{
		DSL_TYPE_MAT_OPAQUE,
		DSL_TYPE_MAT_EM,
		DSL_TYPE_SKY,
		DSL_TYPE_TR,
		DSL_LIGHT_OMNI,
		DSL_TYPE_SKYBOX,
		DSL_TYPE_TERRAIN,
		DSL_TYPE_TERRAIN_COMPUTE,
		DSL_TYPE_WATER_COMPUTE,
		DSL_TYPE_WATER,
		DSL_TYPE_COUNT
	};

	enum SAMPLER_TYPE
	{
		SAMPLER_TYPE_CLAMP,
		SAMPLER_TYPE_REPEATE,
		SAMPLER_TYPE_COUNT
	};

	struct base_resource
	{
		static const uint32_t data_size=256;
		static_assert(data_size >= MAX_RESOURCE_SIZE);

		char data[data_size];
		uint32_t res_type;
		std::atomic_bool ready_bit;
	};

	struct base_resource_build_info
	{
		static const uint32_t data_size = 128;
		static_assert(data_size>=MAX_BUILD_INFO_SIZE);

		char data[data_size];
		uint32_t resource_type;
		base_resource* base_res;
	};


	template<size_t res_type>
	struct resource;


	template<>
	struct resource<RES_TYPE_MAT_OPAQUE>
	{
		struct build_info
		{
			const char* texfiles[TEX_TYPE_COUNT];// [128];
			glm::vec3 color;
			float roughness;
			float metal;
			float height_scale;
			uint32_t tex_flags;
		};

		struct texture
		{
			VkImage image;
			VkImageView view;
			VkSampler sampler;
			uint64_t offset;
		};

		struct data
		{
			glm::vec3 color;
			float roughness;
			float metal;
			float height_scale;
			uint32_t flags;
		};

		texture texs[TEX_TYPE_COUNT];
		VkDescriptorSet ds;
		uint64_t data_offset;
		VkBuffer data_buffer;
		uint32_t dp_index;
	};

	template<>
	struct resource<RES_TYPE_MESH>
	{
		struct build_info
		{
			const char* filename;
			bool calc_tb;
		};


		VkBuffer vb; //vertex
		VkBuffer ib; //index
		VkBuffer veb; //vertex ext
		uint64_t vb_offset;
		uint64_t ib_offset;
		uint64_t veb_offset;
		uint64_t size;
	};

	template<>
	struct resource<RES_TYPE_TR>
	{
		struct build_info
		{
			glm::mat4 model;
			glm::vec3 scale;
			glm::vec2 tex_scale;
		};

		struct data
		{
			glm::mat4 model;
			glm::vec3 scale;
			uint32_t padding0;
			glm::vec2 tex_scale;
			uint32_t padding1[2];
		};

		VkDescriptorSet ds;
		uint32_t dp_index;
		VkBuffer data_buffer;
		uint64_t data_offset;
	};

	template<>
	struct resource<RES_TYPE_SKY>
	{
		struct build_info
		{
			const char* filename;
			glm::uvec3 sky_image_size;
			glm::uvec2 transmittance_image_size;
		};

		struct texture
		{
			VkImage image;
			VkImageView view;
			uint64_t offset;
			VkSampler sampler;
		};


		VkDescriptorSet ds;
		texture tex[3];
		uint32_t dp_index;
	};

	template<>
	struct resource<RES_TYPE_TERRAIN>
	{
		static const uint32_t max_request_count = 128;

		struct build_info
		{
			const char* filename;
			uint32_t mip_level_count;
			glm::uvec2 level0_image_size;
			glm::uvec2 level0_tile_size;
			glm::vec3 size_in_meters;
		};

		struct data
		{
			glm::vec2 terrain_size_in_meters;
			glm::vec2 meter_per_tile_size_length;
			glm::ivec2 tile_count;
			float mip_level_count;
			float height_scale;
		};

		struct request_data
		{
			glm::vec2 tile_size_in_meter;
			float mip_level_count;
		};

		struct texture
		{
			VkImage image;
			VkImageView view;
			VkSampler sampler;
			uint64_t mip_tail_offset;
			uint64_t dummy_page_offset;
			vector<std::ifstream> files;
		};

		texture tex;
		VkDescriptorSet ds;
		VkDescriptorSet request_ds;
		uint32_t dp_index;
		glm::uvec2 level0_tile_size;
		glm::uvec2 tile_count;

		VkBuffer data_buffer;
		uint64_t data_offset;

		VkBuffer request_data_buffer;
		uint64_t request_data_offset;
	};

	template<>
	struct resource<RES_TYPE_WATER>
	{
		static const uint32_t GRID_SIZE = 1024;

		struct build_info
		{
			const char* filename;
			glm::vec2 grid_size_in_meters;
			float base_frequency;
			float A;
		};

		struct fft_params_data
		{
			glm::vec2 two_pi_per_L; //l=grid side length in meters
			float sqrtA;
			float base_frequency;
		};

		struct texture
		{
			VkImage image;
			VkImageView view;
			uint64_t offset;
			VkSampler sampler;
		};

		VkDescriptorSet ds;
		VkDescriptorSet fft_ds;

		texture noise;
		texture tex;

		VkBuffer fft_params_buffer;
		uint64_t fft_params_offset;

		glm::vec2 grid_size_in_meters;

		uint32_t dp_index;
	};

	template<uint32_t rend_type>
	struct renderable;

	template<>
	struct renderable<REND_TYPE_OPAQUE_OBJECT>
	{
		VkDescriptorSet tr_ds;
		VkDescriptorSet mat_opaque_ds;
		uint32_t mesh_index_size;
		VkBuffer mesh_vb;
		VkBuffer mesh_ib;
		VkBuffer mesh_veb;
	};

	template<>
	struct renderable<REND_TYPE_WATER>
	{
		VkDescriptorSet ds;
		VkDescriptorSet fft_ds;
		glm::vec2 grid_size_in_meters;
	};

	template<>
	struct renderable<REND_TYPE_SKY>
	{
		VkDescriptorSet ds;
	};

	template<>
	struct renderable<REND_TYPE_TERRAIN>
	{
		VkDescriptorSet ds;
		VkDescriptorSet request_ds;
		array<VkDescriptorSet, 4> opaque_material_dss;
	};


	class dp_pool
	{
		struct dp
		{
			VkDescriptorPool pool;
			uint32_t free_count;
		};

	public:
		dp_pool(uint32_t pool_sizes_count, uint32_t dp_capacity, VkDevice device, memory_resource* memory) :
			m_pool_sizes(memory, pool_sizes_count),
			m_dps(memory),
			m_create_info{},
			m_device(device)
		{
			m_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			m_create_info.maxSets = dp_capacity;
			m_create_info.pPoolSizes = m_pool_sizes.data();
			m_create_info.poolSizeCount = pool_sizes_count;
		}

		VkDescriptorPoolSize* sizes()
		{
			return m_pool_sizes.data();
		}

		VkDescriptorPool use_dp(uint32_t& dp_id)
		{
			dp_id = 0;
			for (auto it=m_dps.begin(); it!=m_dps.end(); ++it)
			{
				if (it->free_count > 0)
				{
					--it->free_count;
					return it->pool;
				}
				++dp_id;
			}

			auto new_dp = m_dps.push_back();
			new_dp->free_count = m_create_info.maxSets - 1;

			assert(vkCreateDescriptorPool(m_device, &m_create_info, m_vk_alloc, &new_dp->pool) == VK_SUCCESS);

			return new_dp->pool;
		}

		VkDescriptorPool stop_using_dp(uint32_t dp_id)
		{
			++m_dps[dp_id].free_count;
			return m_dps[dp_id].pool;
		}

	private:
		VkDevice m_device;
		const VkAllocationCallbacks* m_vk_alloc;
		vector<dp> m_dps;
		vector<VkDescriptorPoolSize> m_pool_sizes;
		VkDescriptorPoolCreateInfo m_create_info;	
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

	void read_file2(const char* filename, char* dst)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		assert(file.is_open());

		uint64_t size = (uint64_t)file.tellg();

		file.seekg(0);
		file.read(dst, size);
		file.close();
	}

	template<uint32_t res_type>
	constexpr uint32_t max_resource_size(uint32_t val)
	{
		if constexpr (res_type == RES_TYPE_COUNT)
			return val;
		else
		{
			val = val < sizeof(resource<res_type>) ? sizeof(resource<res_type>) : val;
			val = max_resource_size<res_type + 1>(val);
			return val;
		}
	}

	template<uint32_t res_type>
	constexpr uint32_t max_build_info_size(uint32_t val)
	{
		if constexpr (res_type == RES_TYPE_COUNT)
			return val;
		else
		{
			val = val < sizeof(resource<res_type>::build_info) ? sizeof(resource<res_type>::build_info) : val;
			val = max_build_info_size<res_type + 1>(val);
			return val;
		}
	}

	constexpr uint32_t MAX_RESOURCE_SIZE = max_resource_size<0>(0);
	constexpr uint32_t MAX_BUILD_INFO_SIZE = max_build_info_size<0>(0);

} //namespace rcq