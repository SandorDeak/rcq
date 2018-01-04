#pragma once

#include "glm.h"

#include "const_frustum_split_count.h"

#include "enum_res_data.h"

#include <array>

namespace rcq
{
	template<uint32_t res_data>
	struct resource_data;

	template<>
	struct resource_data<RES_DATA_ENVIRONMENT_MAP_GEN_MAT>
	{
		glm::mat4 view;
		glm::vec3 dir; //in worlds space
		uint32_t padding0;
		glm::vec3 irradiance;
		uint32_t padding1;
		glm::vec3 ambient_irradiance;
		uint32_t padding2;
	};
	template<>
	struct resource_data<RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX>
	{
		glm::mat4 view;
	};
	template<>
	struct resource_data<RES_DATA_GBUFFER_GEN>
	{
		glm::mat4 proj_x_view;
		glm::vec3 cam_pos;
	};
	template<>
	struct resource_data<RES_DATA_DIR_SHADOW_MAP_GEN>
	{
		glm::mat4 projs[FRUSTUM_SPLIT_COUNT];
	};
	template<>
	struct resource_data<RES_DATA_SS_DIR_SHADOW_MAP_GEN>
	{
		glm::mat4 projs[FRUSTUM_SPLIT_COUNT];
		glm::mat4 view;
		glm::vec3 light_dir;
		float near;
		float far;
	};
	template<>
	struct resource_data<RES_DATA_IMAGE_ASSEMBLER>
	{
		glm::mat4 previous_proj_x_view;
		glm::vec3 previous_view_pos;
		uint32_t padding0;
		glm::vec3 dir; //in view space
		float height_bias;
		glm::vec3 irradiance;
		uint32_t padding1;
		glm::vec3 ambient_irradiance;
		uint32_t padding2;
		glm::vec3 cam_pos;
		uint32_t padding3;
	};
	template<>
	struct resource_data<RES_DATA_SKY_DRAWER>
	{
		glm::mat4 proj_x_view_at_origin;
		glm::vec3 light_dir;
		float height;
		glm::vec3 irradiance;
		uint32_t padding0;
	};
	template<>
	struct resource_data<RES_DATA_SUN_DRAWER>
	{
		glm::mat4 proj_x_view_at_origin;
		glm::vec3 light_dir;
		uint32_t padding0;
		glm::vec3 helper_dir;
		float height;
		glm::vec3 irradiance;
		uint32_t padding1;
	};
	template<>
	struct resource_data<RES_DATA_TERRAIN_DRAWER>
	{
		glm::mat4 proj_x_view;
		glm::vec3 view_pos;
		uint32_t padding0;
	};
	template<>
	struct resource_data<RES_DATA_TERRAIN_TILE_REQUEST>
	{
		glm::vec3 view_pos;
		float near;
		float far;
		uint32_t padding0[3];
	};
	template<>
	struct resource_data<RES_DATA_WATER_DRAWER>
	{
		glm::mat4 proj_x_view;
		glm::mat4 mirrored_proj_x_view;
		glm::vec3 view_pos;
		float height_bias;
		glm::vec3 light_dir;
		uint32_t padding0;
		glm::vec3 irradiance;
		uint32_t padding1;
		glm::vec3 ambient_irradiance;
		uint32_t padding2;
		glm::vec2 tile_offset;
		glm::vec2 tile_size_in_meter;
		glm::vec2 half_resolution;
	};
	template<>
	struct resource_data<RES_DATA_REFRACTION_MAP_GEN>
	{
		glm::mat4 proj_x_view;
		glm::vec3 view_pos_at_ground;
		float far;
	};
	template<>
	struct resource_data<RES_DATA_SSR_RAY_CASTING>
	{
		glm::mat4 proj_x_view;
		glm::vec3 view_pos;
		float ray_length;
	};
	template<>
	struct resource_data<RES_DATA_WATER_COMPUTE>
	{
		glm::vec2 wind_dir;
		float one_over_wind_speed_to_the_4;
		float time;
	};


	struct res_data
	{
		size_t data[RES_DATA_COUNT];

		VkDeviceSize buffer_offset;
		VkBuffer buffer;
		VkDeviceSize staging_buffer_offset;
		VkBuffer staging_buffer;
		size_t size;
		std::array<size_t, RES_DATA_COUNT> offsets;

		template<size_t... res_data>
		static constexpr auto get_sizes(std::index_sequence<res_data...>)
		{
			return std::array<size_t, sizeof...(res_data)>{ sizeof(resource_data<res_data>) ... };
		}

		void calc_offset_and_size(size_t alignment)
		{
			constexpr auto res_data_sizes = get_sizes(std::make_index_sequence<RES_DATA_COUNT>());

			size = 0;
			for (uint32_t i = 0; i < RES_DATA_COUNT; ++i)
			{
				offsets[i] = size;
				size += align(alignment, res_data_sizes[i]);
			}
		}

		static size_t align(size_t alignment, size_t offset)
		{
			return (offset + alignment - 1) & (~(alignment - 1));
		}


		//template<size_t... indices>
		void set_pointers(size_t base/*, std::index_sequence<indices...>*/)
		{
			for (uint32_t i = 0; i<RES_DATA_COUNT; ++i)
			{
				data[i] = base + offsets[i];
			}
			/*
			auto l = { ([](auto&& p, char* val) {
			p = reinterpret_cast<std::remove_reference_t<decltype(p)>>(val);
			}(std::get<indices>(data), base + std::get<indices>(offsets))
			, 0)... };*/
		}
		template<uint32_t res_data>
		auto get()
		{
			return reinterpret_cast<resource_data<res_data>*>(data[res_data]);
		}
	};
}