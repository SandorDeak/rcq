#include "scene.h"

#include "engine.h"


scene::scene(const glm::vec2& window_size) : m_window_size(window_size)
{
	build();
}


scene::~scene()
{
	for (auto& m : m_meshes)
		rcq::engine::instance()->cmd_destroy<rcq::RESOURCE_TYPE_MESH>(m.id);
	for (auto& mat : m_mats)
		rcq::engine::instance()->cmd_destroy<rcq::RESOURCE_TYPE_MAT>(mat.id);
	for (auto& tr : m_trs)
		rcq::engine::instance()->cmd_destroy<rcq::RESOURCE_TYPE_TR>(tr.id);
	for (auto& e : m_entities)
		rcq::engine::instance()->cmd_destroy_renderable(e.m_id);

	rcq::engine::instance()->cmd_dispatch();
}

void scene::build()
{
	//create meshes
	mesh m;
	m.resource = "meshes/buddha/buddha.obj";
	m.calc_tb = false;
	m.id = MESH_BUDDHA;

	m_meshes.push_back(m);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MESH>(m.id, m.resource, m.calc_tb);


	//create materials
	material mat;
	mat.data.flags = 0;
	mat.data.metal = 1.f;
	mat.data.roughness = 0.1f;
	mat.data.color = { 1.f, 0.71f, 0.29f };
	mat.id = MAT_GOLD;
	mat.type = rcq::MAT_TYPE_BASIC;

	m_mats.push_back(mat);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MAT>(mat.id, mat.data, mat.tex_resources, mat.type);

	//create transforms
	transform tr;
	tr.data.model = glm::mat4(1.f);
	tr.data.scale = glm::vec3(1.f);
	tr.id = TR_GOLD_BUDDHA;
	tr.usage = rcq::USAGE_STATIC;

	for (int i = 0; i < 10; ++i)
	{
		tr.data.model = glm::translate(glm::mat4(1.f), glm::vec3(float(i - 5)*0.5f, 0.f, 0.f));
		tr.id = i;
		m_trs.push_back(tr);
		rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_TR>(tr.id, tr.data, tr.usage);
	}

	//create entities
	entity buddha;
	buddha.m_material_id = MAT_GOLD;
	buddha.m_mesh_id = MESH_BUDDHA;

	for (int i = 0; i < 10; ++i)
	{
		buddha.m_transform_id = i;
		buddha.m_id = i;
		m_entities.push_back(buddha);
		rcq::engine::instance()->cmd_build_renderable(buddha.m_id, buddha.m_transform_id, buddha.m_mesh_id, buddha.m_material_id,
			rcq::LIFE_EXPECTANCY_LONG);
	}
	rcq::engine::instance()->cmd_dispatch();

	m_camera.proj = glm::perspective(glm::radians(45.f), m_window_size.x / m_window_size.y, 0.1f, 50.f);
	m_camera.proj[1][1] *= (-1);
	m_camera.view = glm::lookAt(glm::vec3(2.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
	m_camera.data.pos = glm::vec3(2.f);
	m_camera.data.proj_x_view = m_camera.proj*m_camera.view;

}

void scene::update(float dt)
{
	static float elapsed_time = 0.f;
	elapsed_time += dt;

	for (int i = 0; i < 10; ++i)
	{
		m_trs[i].data.model = glm::rotate(m_trs[i].data.model, glm::radians(90.f*dt), glm::vec3(0.f, 1.f, 0.f));
		rcq::engine::instance()->cmd_update_transform(m_trs[i].id, m_trs[i].data);
	}
	rcq::engine::instance()->cmd_update_camera(m_camera.data);

	rcq::engine::instance()->cmd_render();
	rcq::engine::instance()->cmd_dispatch();
}