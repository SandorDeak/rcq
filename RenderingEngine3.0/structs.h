#pragma once

#include "foundation.h"

enum MESH : rcq::unique_id
{
	MESH_FLOOR,
	MESH_SHELF,
	MESH_BUDDHA,
	MESH_CUBE_INSIDE_OUT,
	MESH_COUNT
};

enum MAT : rcq::unique_id
{
	MAT_OAKFLOOR,
	MAT_BAMBOO_WOOD,
	MAT_GOLD,
	MAT_COUNT
};

/*enum TR : rcq::unique_id
{
	TR_GOLD_BUDDHA,
	TR_COUNT
};*/

enum ENTITIY : rcq::unique_id
{
	ENTITY_FLOOR,
	ENTITY_SHELF,
	ENTITY_WALLS,
	ENTITY_GOLD_BUDDHA,
	ENTITY_LIGHT,
	ENTITY_COUNT
};

struct material_opaque
{

	rcq::material_opaque_data data;
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
	glm::vec3 pos;
	glm::vec3 look_dir;
	rcq::camera_data data;
};

struct light_omni
{
	rcq::light_omni_data data;
	rcq::unique_id id;
};