#include "scene.h"

#include "engine.h"

const int buddha_count = 3;


scene::scene(GLFWwindow* window, const glm::vec2& window_size) : m_window(window), m_window_size(window_size)
{
	build();
}


scene::~scene()
{
	rcq::engine::instance()->cmd_destroy<rcq::RESOURCE_TYPE_SKYBOX>(m_skybox.id);

	for (auto& m : m_meshes)
		rcq::engine::instance()->cmd_destroy<rcq::RESOURCE_TYPE_MESH>(m.id);
	for (auto& mat : m_mats)
		rcq::engine::instance()->cmd_destroy<rcq::RESOURCE_TYPE_MAT_OPAQUE>(mat.id);
	for (auto& tr : m_trs)
		rcq::engine::instance()->cmd_destroy<rcq::RESOURCE_TYPE_TR>(tr.id);
	for (auto& e : m_entities)
		rcq::engine::instance()->cmd_destroy_renderable(e.m_id, e.m_rend_type, e.m_life_exp);
	for (auto& res : m_light_omni)
		rcq::engine::instance()->cmd_destroy<rcq::RESOURCE_TYPE_LIGHT_OMNI>(res.id);

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
	m.id = MESH_SHELF;
	m_meshes.push_back(m);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MESH>(m.id, m.resource, m.calc_tb);

	m.resource = "meshes/cube_inside_out/cube_inside_out.obj";
	m.calc_tb = false;
	m.id = MESH_CUBE_INSIDE_OUT;
	m_meshes.push_back(m);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MESH>(m.id, m.resource, m.calc_tb);

	//create materials
	material_opaque mat;
	//gold
	mat.data.flags = 0;
	mat.data.metal = 1.f;
	mat.data.roughness = 0.1f;
	mat.data.color = { 1.f, 0.71f, 0.29f };
	mat.id = MAT_GOLD;
	m_mats.push_back(mat);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MAT_OPAQUE>(mat.id, mat.data, mat.tex_resources);

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
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MAT_OPAQUE>(mat.id, mat.data, mat.tex_resources);

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
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_MAT_OPAQUE>(mat.id, mat.data, mat.tex_resources);

	//create transforms
	transform tr;
	//buddha
	tr.data.model = glm::mat4(1.f);
	tr.data.scale = glm::vec3(1.f);
	tr.id = ENTITY_GOLD_BUDDHA;
	tr.usage = rcq::USAGE_DYNAMIC;

	for (int i = 0; i < buddha_count; ++i)
	{
		tr.data.model = glm::translate(glm::mat4(1.f), glm::vec3(float(i - 5)*0.5f, -0.5f, 0.f));
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
	tr.data.model= glm::translate(glm::mat4(1.f), { 0.f, 0.f, -4.f });
	tr.data.scale = glm::vec3(0.015f);
	tr.data.tex_scale = glm::vec2(5.f);
	tr.id = ENTITY_SHELF;
	tr.usage = rcq::USAGE_STATIC;
	m_trs.push_back(tr);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_TR>(tr.id, tr.data, tr.usage);

	//walls
	tr.data.model = glm::translate(glm::mat4(1.f), { 0.f, 1.f, 0.f });
	tr.data.scale = glm::vec3(6.f, 2.f, 6.f);
	tr.id = ENTITY_WALLS;
	m_trs.push_back(tr);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_TR>(tr.id, tr.data, tr.usage);

	//create light res
	rcq::light_omni_data old;
	old.pos = glm::vec3(2.f);
	old.flags = rcq::LIGHT_FLAG_SHADOW_MAP;
	old.radiance = glm::vec3(3.f);

	light_omni lo;
	lo.data = old;
	lo.id=0;
	m_light_omni.push_back(lo);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_LIGHT_OMNI>(lo.id, lo.data, rcq::USAGE_STATIC);

	old.pos = glm::vec3(-2.f, -2.f, 2.f);
	old.radiance = glm::vec3(8.f);
	old.flags = 0;
	lo.data = old;
	lo.id = 1;
	m_light_omni.push_back(lo);
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_LIGHT_OMNI>(lo.id, lo.data, rcq::USAGE_STATIC);

	//create lights
	entity l;
	l.m_id = ENTITY_LIGHT0;
	l.m_life_exp = rcq::LIFE_EXPECTANCY_LONG;
	l.m_material_light_id = 0;
	l.m_rend_type = rcq::RENDERABLE_TYPE_LIGHT_OMNI;
	m_entities.push_back(l);
	rcq::engine::instance()->cmd_build_renderable(l.m_id, 0, 0, l.m_material_light_id, l.m_rend_type, l.m_life_exp);

	l.m_id = ENTITY_LIGHT1;
	l.m_life_exp = rcq::LIFE_EXPECTANCY_LONG;
	l.m_material_light_id = 1;
	l.m_rend_type = rcq::RENDERABLE_TYPE_LIGHT_OMNI;
	m_entities.push_back(l);
	rcq::engine::instance()->cmd_build_renderable(l.m_id, 0, 0, l.m_material_light_id, l.m_rend_type, l.m_life_exp);

	//create skybox res
	m_skybox.resource = "textures/skybox";
	m_skybox.id = SKYBOX_WATER;
	rcq::engine::instance()->cmd_build<rcq::RESOURCE_TYPE_SKYBOX>(m_skybox.id, m_skybox.resource);

	//create skybox
	entity sb;
	sb.m_id = ENTITY_SKYBOX;
	sb.m_material_light_id = SKYBOX_WATER;
	sb.m_life_exp = rcq::LIFE_EXPECTANCY_LONG;
	sb.m_rend_type = rcq::RENDERABLE_TYPE_SKYBOX;
	m_entities.push_back(sb);
	rcq::engine::instance()->cmd_build_renderable(sb.m_id, 0, 0, sb.m_material_light_id, sb.m_rend_type, sb.m_life_exp);


	//create entities
	entity e;
	//buddha
	e.m_material_light_id = MAT_GOLD;
	e.m_mesh_id = MESH_BUDDHA;
	e.m_life_exp=rcq::LIFE_EXPECTANCY_LONG;
	e.m_rend_type = rcq::RENDERABLE_TYPE_MAT_OPAQUE;

	for (int i = 0; i < buddha_count; ++i)
	{
		e.m_transform_id = ENTITY_GOLD_BUDDHA+i;
		e.m_id = ENTITY_GOLD_BUDDHA + i;
		m_entities.push_back(e);
		rcq::engine::instance()->cmd_build_renderable(e.m_id, e.m_transform_id, e.m_mesh_id, e.m_material_light_id, e.m_rend_type,
			e.m_life_exp);
	}

	//floor
	e.m_material_light_id = MAT_OAKFLOOR;
	e.m_mesh_id = MESH_FLOOR;
	e.m_transform_id = ENTITY_FLOOR;
	e.m_id = ENTITY_FLOOR;
	m_entities.push_back(e);
	rcq::engine::instance()->cmd_build_renderable(e.m_id, e.m_transform_id, e.m_mesh_id, e.m_material_light_id, e.m_rend_type,
		e.m_life_exp);

	//self
	e.m_material_light_id = MAT_BAMBOO_WOOD;
	e.m_mesh_id = MESH_SHELF;
	e.m_transform_id = ENTITY_SHELF;
	e.m_id = ENTITY_SHELF;
	m_entities.push_back(e);
	rcq::engine::instance()->cmd_build_renderable(e.m_id, e.m_transform_id, e.m_mesh_id, e.m_material_light_id, e.m_rend_type,
		e.m_life_exp);

	//wall
	e.m_material_light_id = MAT_GOLD;
	e.m_mesh_id = MESH_CUBE_INSIDE_OUT;
	e.m_transform_id = ENTITY_WALLS;
	e.m_id = ENTITY_WALLS;
	m_entities.push_back(e);
	rcq::engine::instance()->cmd_build_renderable(e.m_id, e.m_transform_id, e.m_mesh_id, e.m_material_light_id, e.m_rend_type,
		e.m_life_exp);

	//dispatch
	rcq::engine::instance()->cmd_dispatch();

	//set camera
	m_camera.pos = glm::vec3(2.f);
	m_camera.look_dir = glm::normalize(glm::vec3(-2.f));
	m_camera.proj = glm::perspective(glm::radians(45.f), m_window_size.x / m_window_size.y, 0.1f, 50.f);
	m_camera.proj[1][1] *= (-1);
	//m_camera.proj[3][2] *= (-1);
	m_camera.view = glm::lookAt(glm::vec3(2.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
	m_camera.data.pos = glm::vec3(2.f);
	m_camera.data.proj_x_view = m_camera.proj*m_camera.view;

}

void scene::update(float dt)
{
	static float elapsed_time = 0.f;
	elapsed_time += dt;

	for (int i = 0; i < buddha_count; ++i)
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