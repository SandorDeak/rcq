#pragma once

#include "vulkan.h"
#include "glm.h"

#include "vector.h"

#include "enum_res_type.h"
#include "enum_tex_type.h"

#include <atomic>

namespace rcq
{
	template<uint32_t res_type>
	struct resource;

	template<>
	struct resource<RES_TYPE_MAT_OPAQUE>
	{
		struct build_info
		{
			const char* texfiles[TEX_TYPE_COUNT];
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
			uint32_t padding0;
			float roughness;
			float metal;
			float height_scale;
			uint32_t flags;
		};

		texture texs[TEX_TYPE_COUNT];
		VkDescriptorSet ds;
		VkDeviceSize data_offset;
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
		VkDeviceSize vb_offset;
		VkDeviceSize ib_offset;
		VkDeviceSize veb_offset;
		uint32_t size;
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
		VkDeviceSize data_offset;
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
		};

		VkSampler sampler;
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
			VkDeviceSize mip_tail_offset;
			VkDeviceSize dummy_page_offset;
			vector<std::ifstream> files;
		};

		texture tex;
		VkDescriptorSet ds;
		VkDescriptorSet request_ds;
		uint32_t dp_index;
		glm::uvec2 level0_tile_size;
		glm::uvec2 tile_count;
		uint32_t mip_level_count;

		VkBuffer data_buffer;
		VkDeviceSize data_offset;

		VkBuffer request_data_buffer;
		VkDeviceSize request_data_offset;
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
			VkDeviceSize offset;
		};

		VkDescriptorSet ds;
		VkDescriptorSet fft_ds;

		VkSampler sampler;

		texture noise;
		texture tex;

		VkBuffer fft_params_buffer;
		VkDeviceSize fft_params_offset;

		glm::vec2 grid_size_in_meters;

		uint32_t dp_index;
	};


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

	struct base_resource
	{
		static const uint32_t data_size = 256;
		static_assert(data_size >= MAX_RESOURCE_SIZE);

		char data[data_size];
		uint32_t res_type;
		std::atomic_bool ready_bit;
	};

	struct base_resource_build_info
	{
		static const uint32_t data_size = 128;
		static_assert(data_size >= MAX_BUILD_INFO_SIZE);

		char data[data_size];
		uint32_t resource_type;
		base_resource* base_res;
	};
}