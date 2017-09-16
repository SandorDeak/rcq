#pragma once

#include "foundation.h"


struct material
{

	rcq::material_data data;
	uint32_t type;
	std::string tex_resources[rcq::TEX_TYPE_COUNT];
};

struct transform
{
	rcq::transform_data data;
	rcq::USAGE usage;
};

struct mesh
{
	std::string resource;
};

struct camera
{
	glm::mat4 proj;
	glm::mat4 view;
	rcq::camera_data data;
};