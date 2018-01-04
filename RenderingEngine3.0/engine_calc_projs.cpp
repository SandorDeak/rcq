#include "engine.h"

using namespace rcq;

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


void engine::calc_projs()
{
	glm::vec3 light_dir = static_cast<glm::mat3>(m_render_settings.view)*m_render_settings.light_dir;
	glm::mat4 light = glm::lookAtLH(glm::vec3(0.f), light_dir, glm::vec3(0.f, 1.f, 0.f));
	std::array<std::array<glm::vec3, 4>, FRUSTUM_SPLIT_COUNT + 1> frustum_points;

	glm::vec2 max[2];
	glm::vec2 min[2];

	for (uint32_t i = 0; i < FRUSTUM_SPLIT_COUNT + 1; ++i)
	{
		float z = m_render_settings.near + std::powf(static_cast<float>(i) / static_cast<float>(FRUSTUM_SPLIT_COUNT), 3.f)*
			(m_render_settings.far - m_render_settings.near);
		z *= (-1.f);

		std::array<float, 4> x = { -1.f, 1.f, -1.f, 1.f };
		std::array<float, 4> y = { -1.f, -1.f, 1.f, 1.f };

		max[i % 2] = glm::vec2(std::numeric_limits<float>::min());
		min[i % 2] = glm::vec2(std::numeric_limits<float>::max());

		for (uint32_t j = 0; j<4; ++j)
		{
			glm::vec3 v = { x[j] * z / m_render_settings.proj[0][0], y[j] * z / m_render_settings.proj[1][1], z };
			v = static_cast<glm::mat3>(light)*v;
			frustum_points[i][j] = v;

			max[i % 2] = componentwise_max(max[i % 2], { v.x, v.y });
			min[i % 2] = componentwise_min(min[i % 2], { v.x, v.y });
		}

		if (i > 0)
		{
			glm::vec2 frustum_max = componentwise_max(max[0], max[1]);
			glm::vec2 frustum_min = componentwise_min(min[0], min[1]);

			glm::vec2 center = (frustum_max + frustum_min)*0.5f;
			glm::vec2 scale = glm::vec2(2.f) / (frustum_max - frustum_min);

			glm::mat4 proj(1.f);
			proj[0][0] = scale.x;
			proj[1][1] = scale.y;
			proj[2][2] = 0.5f / 1000.f;
			proj[3][0] = -scale.x*center.x;
			proj[3][1] = -scale.y*center.y;
			proj[3][2] = 0.5f;

			m_dir_shadow_projs[i - 1] = proj*light; //from view space
		}
	}
}