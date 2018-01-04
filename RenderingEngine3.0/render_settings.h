#pragma once

#include "glm.h"

namespace rcq
{
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
}