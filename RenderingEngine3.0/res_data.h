#pragma once

#include "glm.h"
#include "const_frustum_split_count.h"
#include "enum_res_data.h"

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
}