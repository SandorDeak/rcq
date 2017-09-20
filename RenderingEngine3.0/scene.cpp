#include "scene.h"

#include "engine.h"


scene::scene()
{
	build();
	init_camera();
}


scene::~scene()
{
	for (auto& m : m_meshes)
		rcq::engine::instance()->cmd_destroy<rcq::RESOURCE_TYPE_MESH>(m.id);
	for (auto& mat : m_mats)
		rcq::engine::instance()->cmd_destroy<rcq::RESOURCE_TYPE_MAT>(mat.id);

	rcq::engine::instance()->cmd_dispatch();
}

void scene::update(float dt)
{

}

void scene::build()
{
	//create meshes
	mesh m;
	m.resource = "meshes/buddha/buddha.obj";
	m.calc_tb = false;
	m.id = rcq::get_unique_id<mesh>();

	m_meshes.push_back(m);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MESH>(m.id, m.resource, m.calc_tb);


	//create materials
	material mat;
	mat.data.flags = 0;
	mat.data.metal = 1.f;
	mat.data.roughness = 0.1f;
	mat.data.color= { 1.f, 0.71f, 0.29f };
	mat.id = rcq::get_unique_id<material>();
	mat.type = rcq::MAT_TYPE_BASIC;

	m_mats.push_back(mat);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MAT>(mat.id, mat.data, mat.tex_resources, mat.type);
	rcq::engine::instance()->cmd_dispatch();

}

void scene::init_camera()
{

}