#pragma once

#include "foundation.h"

enum MESH : rcq::unique_id
{
	MESH_FLOOR,
	MESH_SHELF,
	MESH_BUDDHA,
	MESH_WAREHOUSE,
	MESH_TERRAIN,
	MESH_SPHERE,
	MESH_COUNT
};

enum MAT : rcq::unique_id
{
	MAT_OAKFLOOR,
	MAT_BAMBOO_WOOD,
	MAT_GOLD,
	MAT_TERRAIN,
	MAT_WHITE_WALL,
	MAT_RUSTED_IRON,
	MAT_SCUFFED_ALUMINIUM,
	MAT_COUNT
};

enum SKYBOX : rcq::unique_id
{
	SKYBOX_WATER,
	SKYBOX_COUNT
};

enum SKY : rcq::unique_id
{
	SKY_FIRST,
	SKY_COUNT
};

enum ENTITIY : rcq::unique_id
{
	ENTITY_FLOOR,
	ENTITY_SHELF,
	ENTITY_WAREHOUSE,
	ENTITY_LIGHT0,
	ENTITY_LIGHT1,
	ENTITY_SKYBOX,
	ENTITY_SKY,
	ENTITY_TERRAIN,
	ENTITY_GOLD_BUDDHA,
	ENTITY_RUSTED_IRON_SPHERE,
	ENTITY_SCUFFED_ALUMINIUM_SPHERE,
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

struct skybox
{
	std::string resource;
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