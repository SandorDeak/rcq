#pragma once

#include "foundation.h"
#include "containers.h"


struct material
{
	material(rcq::USAGE u) : usage(u), data(rcq::containers::instance()->new_mat(u)) {}
	~material() { rcq::containers::instance()->delete_mat(data, usage); }

	rcq::material_data* const data;
	uint32_t type;
	std::string tex_resources[rcq::TEX_TYPE_COUNT];
	const rcq::USAGE usage;
};

struct transform
{
	transform(rcq::USAGE u): usage(u), data(rcq::containers::instance()->new_tr(u)) {} 
	~transform() { rcq::containers::instance()->delete_tr(data, usage); }

	rcq::transform_data* const data;
	const rcq::USAGE usage;
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