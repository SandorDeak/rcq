#include "engine.h"

#include "enum_res_data.h"

using namespace rcq;

void engine::process_render_settings()
{
	float height_bias = 1000.f;
	float height = height_bias + m_render_settings.pos.y;

	//environment map gen mat
	{
		auto data = m_res_data.get<RES_DATA_ENVIRONMENT_MAP_GEN_MAT>();
		data->ambient_irradiance = m_render_settings.ambient_irradiance;
		data->dir = m_render_settings.light_dir;
		data->irradiance = m_render_settings.irradiance;
		data->view = m_render_settings.view;
	}

	//environment map gen skybox
	{
		auto data = m_res_data.get<RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX>();
		data->view = m_render_settings.view;
	}

	//dir shadow map gen
	{
		auto data = m_res_data.get<RES_DATA_DIR_SHADOW_MAP_GEN>();
		glm::mat4 projs_from_world[FRUSTUM_SPLIT_COUNT];
		for (uint32_t i = 0; i < FRUSTUM_SPLIT_COUNT; ++i)
			projs_from_world[i] = m_dir_shadow_projs[i] * m_render_settings.view;
		memcpy(data->projs, projs_from_world, sizeof(glm::mat4)*FRUSTUM_SPLIT_COUNT);
	}

	//gbuffer gen
	{
		auto data = m_res_data.get<RES_DATA_GBUFFER_GEN>();
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->cam_pos = m_render_settings.pos;
	}

	//ss dir shadow map gen
	{
		auto data = m_res_data.get<RES_DATA_SS_DIR_SHADOW_MAP_GEN>();
		data->light_dir = m_render_settings.light_dir;
		data->view = m_render_settings.view;
		data->far = m_render_settings.far;
		data->near = m_render_settings.near;
		memcpy(data->projs, m_dir_shadow_projs, sizeof(glm::mat4)*FRUSTUM_SPLIT_COUNT);
	}

	//image assembler data
	{
		auto data = m_res_data.get<RES_DATA_IMAGE_ASSEMBLER>();
		data->ambient_irradiance = m_render_settings.ambient_irradiance;
		data->dir = m_render_settings.light_dir;
		data->irradiance = m_render_settings.irradiance;
		data->cam_pos = m_render_settings.pos;
		data->height_bias = height_bias;
		data->previous_proj_x_view = m_previous_proj_x_view;
		data->previous_view_pos = m_previous_view_pos;
	}

	glm::mat4 view_at_origin = m_render_settings.view;
	view_at_origin[3][0] = 0.f;
	view_at_origin[3][1] = 0.f;
	view_at_origin[3][2] = 0.f;
	glm::mat4 proj_x_view_at_origin = m_render_settings.proj*view_at_origin;

	//sky drawer data
	{
		auto data = m_res_data.get<RES_DATA_SKY_DRAWER>();

		data->proj_x_view_at_origin = proj_x_view_at_origin;
		data->height = height;
		data->irradiance = m_render_settings.irradiance;
		data->light_dir = m_render_settings.light_dir;
	}

	//sun drawer data
	{
		auto data = m_res_data.get<RES_DATA_SUN_DRAWER>();
		data->height = height;
		data->irradiance = m_render_settings.irradiance;
		data->light_dir = m_render_settings.light_dir;
		data->proj_x_view_at_origin = proj_x_view_at_origin;
		data->helper_dir = get_orthonormal(m_render_settings.light_dir);
	}

	//terrain drawer data
	{
		auto data = m_res_data.get<RES_DATA_TERRAIN_DRAWER>();
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->view_pos = m_render_settings.pos;
	}

	//terrain tile request data
	{
		auto data = m_res_data.get<RES_DATA_TERRAIN_TILE_REQUEST>();
		data->far = m_render_settings.far;
		data->near = m_render_settings.near;
		data->view_pos = m_render_settings.pos;
	}

	//water drawer data
	{
		auto data = m_res_data.get<RES_DATA_WATER_DRAWER>();
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->half_resolution.x = static_cast<float>(m_base.swap_chain_image_extent.width) / 2.f;
		data->half_resolution.y = static_cast<float>(m_base.swap_chain_image_extent.height) / 2.f;
		data->light_dir = m_render_settings.light_dir;
		data->view_pos = m_render_settings.pos;
		data->tile_size_in_meter = m_water.grid_size_in_meters;
		data->tile_offset = (glm::floor(glm::vec2(m_render_settings.pos.x - m_render_settings.far, m_render_settings.pos.z - m_render_settings.far)
			/ m_water.grid_size_in_meters) + glm::vec2(0.5f))*
			m_water.grid_size_in_meters;
		m_water_tiles_count = static_cast<glm::uvec2>(glm::ceil(glm::vec2(2.f*m_render_settings.far) / m_water.grid_size_in_meters));
		data->ambient_irradiance = m_render_settings.ambient_irradiance;
		data->height_bias = height_bias;
		data->mirrored_proj_x_view = glm::mat4(1.f); //CORRECT IT!!!
		data->irradiance = m_render_settings.irradiance;
	}

	//refraction map gen
	{
		auto data = m_res_data.get<RES_DATA_REFRACTION_MAP_GEN>();
		data->far = m_render_settings.far;
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->view_pos_at_ground = { m_render_settings.pos.x, glm::min(0.05f*glm::dot(m_render_settings.wind, m_render_settings.wind) / 9.81f,
			m_render_settings.pos.y - m_render_settings.near - 0.000001f), m_render_settings.pos.z };
	}

	//ssr ray casting
	{
		auto data = m_res_data.get<RES_DATA_SSR_RAY_CASTING>();
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->ray_length = glm::length(glm::vec3(m_render_settings.far / m_render_settings.proj[0][0], m_render_settings.far /
			m_render_settings.proj[1][1], m_render_settings.far));
		data->view_pos = m_render_settings.pos;
	}

	//water compute data
	{
		auto data = m_res_data.get<RES_DATA_WATER_COMPUTE>();
		data->one_over_wind_speed_to_the_4 = 1.f / powf(glm::length(m_render_settings.wind), 4.f);
		data->wind_dir = glm::normalize(m_render_settings.wind);
		data->time = m_render_settings.time;
	}

	m_previous_proj_x_view = m_render_settings.proj*m_render_settings.view;
	m_previous_view_pos = m_render_settings.pos;
}