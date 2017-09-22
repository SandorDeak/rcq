#include "scene.h"

#include "engine.h"


scene::scene(GLFWwindow* window, const glm::vec2& window_size) : m_window(window), m_window_size(window_size)
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

	m.resource = "meshes/plane_with_tex_coords/plane_with_tex_coords.obj";
	m.calc_tb = true;
	m.id = MESH_FLOOR;
	m_meshes.push_back(m);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MESH>(m.id, m.resource, m.calc_tb);

	m.resource = "meshes/shelf/CAB.obj";
	m.id = MESH_SELF;
	m_meshes.push_back(m);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MESH>(m.id, m.resource, m.calc_tb);


	//create materials
	material mat;
	//gold
	mat.data.flags = 0;
	mat.data.metal = 1.f;
	mat.data.roughness = 0.1f;
	mat.data.color = { 1.f, 0.71f, 0.29f };
	mat.id = MAT_GOLD;
	mat.type = rcq::MAT_TYPE_BASIC;
	m_mats.push_back(mat);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MAT>(mat.id, mat.data, mat.tex_resources, mat.type);

	//bamboo wood
	mat.data.flags = rcq::TEX_TYPE_FLAG_COLOR | rcq::TEX_TYPE_FLAG_ROUGHNESS
		| rcq::TEX_TYPE_FLAG_NORMAL | rcq::TEX_TYPE_FLAG_AO;
	mat.data.metal = 0.f;
	mat.id = MAT_BAMBOO_WOOD;
	mat.tex_resources[rcq::TEX_TYPE_COLOR]= "textures/bamboo-wood-semigloss/bamboo-wood-semigloss-albedo.png";
	mat.tex_resources[rcq::TEX_TYPE_ROUGHNESS]= "textures/bamboo-wood-semigloss/bamboo-wood-semigloss-roughness.png";
	mat.tex_resources[rcq::TEX_TYPE_NORMAL]= "textures/bamboo-wood-semigloss/bamboo-wood-semigloss-normal.png";
	mat.tex_resources[rcq::TEX_TYPE_AO]= "textures/bamboo-wood-semigloss/bamboo-wood-semigloss-ao.png";
	m_mats.push_back(mat);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MAT>(mat.id, mat.data, mat.tex_resources, mat.type);

	//oakfloor
	mat.data.flags = rcq::TEX_TYPE_FLAG_COLOR | rcq::TEX_TYPE_FLAG_ROUGHNESS
		| rcq::TEX_TYPE_FLAG_NORMAL | rcq::TEX_TYPE_FLAG_AO | rcq::TEX_TYPE_FLAG_HEIGHT;
	mat.data.height_scale = 0.02f;
	mat.data.metal = 0.f;
	mat.id = MAT_OAKFLOOR;
	mat.tex_resources[rcq::TEX_TYPE_COLOR] = "textures/oakfloor/oakfloor_basecolor.png";
	mat.tex_resources[rcq::TEX_TYPE_ROUGHNESS] = "textures/oakfloor/oakfloor_roughness.png";
	mat.tex_resources[rcq::TEX_TYPE_NORMAL] = "textures/oakfloor/oakfloor_normal.png";
	mat.tex_resources[rcq::TEX_TYPE_AO] = "textures/oakfloor/oakfloor_AO.png";
	mat.tex_resources[rcq::TEX_TYPE_HEIGHT]= "textures/oakfloor/oakfloor_Height.png";
	m_mats.push_back(mat);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MAT>(mat.id, mat.data, mat.tex_resources, mat.type);

	//create transforms
	transform tr;
	//buddha
	tr.data.model = glm::mat4(1.f);
	tr.data.scale = glm::vec3(1.f);
	tr.id = ENTITY_GOLD_BUDDHA;
	tr.usage = rcq::USAGE_DYNAMIC;

	for (int i = 0; i < 10; ++i)
	{
		tr.data.model = glm::translate(glm::mat4(1.f), glm::vec3(float(i - 5)*0.5f, 0.f, 0.f));
		tr.id = ENTITY_GOLD_BUDDHA+i;
		m_trs.push_back(tr);
		rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_TR>(tr.id, tr.data, tr.usage);
	}

	//floor
	tr.data.model= glm::translate(glm::mat4(1.f), { 0.f, -1.f, 0.f });
	tr.data.scale = glm::vec3(6.f);
	tr.data.tex_scale = glm::vec2(1.f);
	tr.id = ENTITY_FLOOR;
	tr.usage = rcq::USAGE_DYNAMIC;
	m_trs.push_back(tr);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_TR>(tr.id, tr.data, tr.usage);

	//self
	tr.data.model= glm::translate(glm::mat4(1.f), { 0.f, 1.f, -4.f });
	tr.data.scale = glm::vec3(0.015f);
	tr.data.tex_scale = glm::vec2(5.f);
	tr.id = ENTITY_SELF;
	m_trs.push_back(tr);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_TR>(tr.id, tr.data, tr.usage);



	//create entities
	entity e;
	//buddha
	e.m_material_id = MAT_GOLD;
	e.m_mesh_id = MESH_BUDDHA;

	for (int i = 0; i < 10; ++i)
	{
		e.m_transform_id = ENTITY_GOLD_BUDDHA+i;
		e.m_id = ENTITY_GOLD_BUDDHA + i;
		m_entities.push_back(e);
		rcq::engine::instance()->cmd_build_renderable(e.m_id, e.m_transform_id, e.m_mesh_id, e.m_material_id,
			rcq::LIFE_EXPECTANCY_LONG);
	}

	//floor
	e.m_material_id = MAT_OAKFLOOR;
	e.m_mesh_id = MESH_FLOOR;
	e.m_transform_id = ENTITY_FLOOR;
	e.m_id = ENTITY_FLOOR;
	m_entities.push_back(e);
	rcq::engine::instance()->cmd_build_renderable(e.m_id, e.m_transform_id, e.m_mesh_id, e.m_material_id,
		rcq::LIFE_EXPECTANCY_LONG);

	//self
	e.m_material_id = MAT_BAMBOO_WOOD;
	e.m_mesh_id = MESH_SELF;
	e.m_transform_id = ENTITY_SELF;
	e.m_id = ENTITY_SELF;
	m_entities.push_back(e);
	rcq::engine::instance()->cmd_build_renderable(e.m_id, e.m_transform_id, e.m_mesh_id, e.m_material_id,
		rcq::LIFE_EXPECTANCY_LONG);

	//dispatch
	rcq::engine::instance()->cmd_dispatch();

	//set camera
	m_camera.pos = glm::vec3(2.f);
	m_camera.look_dir = glm::normalize(glm::vec3(-2.f));
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
	update_camera(dt);
	rcq::engine::instance()->cmd_update_camera(m_camera.data);

	rcq::engine::instance()->cmd_render();
	rcq::engine::instance()->cmd_dispatch();
}

void scene::update_camera(float dt)
{
	glm::vec3 move(0.f);
	float h_rot = 0.f;
	float v_rot = 0.f;

	if (glfwGetKey(m_window, GLFW_KEY_W))
		move.x += 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_S))
		move.x -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_A))
		move.z -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_D))
		move.z += 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_SPACE))
		move.y -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_C))
		move.y += 1.f;

	if (glfwGetKey(m_window, GLFW_KEY_UP))
		v_rot += 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_DOWN))
		v_rot -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_LEFT))
		h_rot -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_RIGHT))
		h_rot += 1.f;

	if (h_rot != 0.f)
	{
		m_camera.look_dir = glm::mat3(glm::rotate(dt*h_rot, glm::vec3(0.f, -1.f, 0.f)))*m_camera.look_dir;
	}

	if (v_rot != 0.f)
	{
		m_camera.look_dir = glm::mat3(glm::rotate(dt*v_rot, glm::vec3(-m_camera.look_dir.z, 0.f, m_camera.look_dir.x)))
			*m_camera.look_dir;
	}

	if (move != glm::vec3(0.f))
	{
		m_camera.pos += dt*glm::mat3(m_camera.look_dir, glm::vec3(0.f, -1.f, 0.f), glm::vec3(-m_camera.look_dir.z, 0.f, m_camera.look_dir.x))
			*glm::normalize(move);
	}

	m_camera.data.pos = m_camera.pos;
	glm::mat4 view = glm::lookAt(m_camera.pos, m_camera.pos + m_camera.look_dir, glm::vec3(0.f, 1.f, 0.f));
	m_camera.data.proj_x_view = m_camera.proj*view;
}