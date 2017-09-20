#pragma once

#include "foundation.h"

enum MESH
{
	MESH_BUDDHA,
	MESH_COUNT
};

enum MAT
{
	MAT_GOLD,
	MAT_COUNT
};

enum ENTITIY
{
	ENTITY_GOLD_BUDDHA,
	ENTITY_COUNT
};

struct material
{

	rcq::material_data data;
	rcq::MAT_TYPE type;
	std::array<std::string, rcq::TEX_TYPE_COUNT> tex_resources;
	rcq::unique_id id;
};

struct transform
{
	rcq::transform_data data;
	rcq::USAGE usage;
	rcq::unique_id id;
};

struct mesh
{
	std::string resource;
	bool calc_tb;
	rcq::unique_id id;
};

struct camera
{
	glm::mat4 proj;
	glm::mat4 view;
	rcq::camera_data data;
};