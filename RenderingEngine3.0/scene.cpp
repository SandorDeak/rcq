#include "scene.h"

#include "engine.h"

constexpr float PI = 3.1415927410125732421875f;

scene::scene(GLFWwindow* window, const glm::vec2& window_size) : m_window(window), m_window_size(window_size)
{
	build();
}


scene::~scene()
{
	rcq_user::destroy_sky();
	rcq_user::destroy_terrain();
	rcq_user::destroy_water();

	for (auto& handle : m_opaque_objects)
		rcq_user::destroy_opaque_object(handle);

	for (auto& handle : m_resources)
		rcq_user::destroy_resource(handle);
	rcq_user::dispatch_resource_destroys();
}

void scene::build()
{
	m_resources.resize(resource::count);

	//create meshes

	//buddha
	rcq_user::build_info<rcq_user::resource::mesh>* mesh_build;
	rcq_user::build_resource<rcq_user::resource::mesh>(&m_resources[resource::mesh_buddha], &mesh_build);
	mesh_build->calc_tb = false;
	mesh_build->filename= "meshes/buddha/buddha.obj";
	//plane
	rcq_user::build_resource<rcq_user::resource::mesh>(&m_resources[resource::mesh_plane], &mesh_build);
	mesh_build->calc_tb = true;
	mesh_build->filename = "meshes/plane_with_tex_coords/plane_with_tex_coords.obj";
	//shelf
	rcq_user::build_resource<rcq_user::resource::mesh>(&m_resources[resource::mesh_shelf], &mesh_build);
	mesh_build->calc_tb = true;
	mesh_build->filename = "meshes/shelf/CAB.obj";
	//sphere
	rcq_user::build_resource<rcq_user::resource::mesh>(&m_resources[resource::mesh_sphere], &mesh_build);
	mesh_build->calc_tb = true;
	mesh_build->filename = "meshes/sphere/sphere.obj";

	//create opaque materials
	rcq_user::build_info<rcq_user::resource::opaque_material>* mat_build_info;
	//gold
	rcq_user::build_resource<rcq_user::resource::opaque_material>(&m_resources[resource::mat_gold], &mat_build_info);
	mat_build_info->color = { 1.f, 0.86f, 0.57f };
	mat_build_info->metal = 1.f;
	mat_build_info->roughness = 0.1f;
	mat_build_info->tex_flags = 0;

	//bamboo wood
	rcq_user::build_resource<rcq_user::resource::opaque_material>(&m_resources[resource::mat_bamboo_wood], &mat_build_info);
	mat_build_info->metal = 0.f;
	mat_build_info->tex_flags = rcq_user::tex_flag::color | rcq_user::tex_flag::roughness | rcq_user::tex_flag::normal |
		rcq_user::tex_flag::ao;
	mat_build_info->texfiles[rcq_user::tex_type::color] = "textures/bamboo-wood-semigloss/bamboo-wood-semigloss-albedo.png";
	mat_build_info->texfiles[rcq_user::tex_type::roughness] = "textures/bamboo-wood-semigloss/bamboo-wood-semigloss-roughness.png";
	mat_build_info->texfiles[rcq_user::tex_type::normal] = "textures/bamboo-wood-semigloss/bamboo-wood-semigloss-normal.png";
	mat_build_info->texfiles[rcq_user::tex_type::ao] = "textures/bamboo-wood-semigloss/bamboo-wood-semigloss-ao.png";
	

	//oakfloor
	rcq_user::build_resource<rcq_user::resource::opaque_material>(&m_resources[resource::mat_oak_floor], &mat_build_info);
	mat_build_info->height_scale = 0.02f;
	mat_build_info->metal = 0.f;
	mat_build_info->tex_flags = rcq_user::tex_flag::color | rcq_user::tex_flag::roughness | rcq_user::tex_flag::normal |
		rcq_user::tex_flag::ao | rcq_user::tex_flag::height;
	mat_build_info->texfiles[rcq_user::tex_type::color] = "textures/oakfloor/oakfloor_basecolor.png";
	mat_build_info->texfiles[rcq_user::tex_type::roughness] = "textures/oakfloor/oakfloor_roughness.png";
	mat_build_info->texfiles[rcq_user::tex_type::normal] = "textures/oakfloor/oakfloor_normal.png";
	mat_build_info->texfiles[rcq_user::tex_type::ao] = "textures/oakfloor/oakfloor_AO.png";
	mat_build_info->texfiles[rcq_user::tex_type::height] = "textures/oakfloor/oakfloor_Height.png";


	//rusted iron
	rcq_user::build_resource<rcq_user::resource::opaque_material>(&m_resources[resource::mat_rusted_iron], &mat_build_info);
	mat_build_info->tex_flags = rcq_user::tex_flag::color | rcq_user::tex_flag::roughness | rcq_user::tex_flag::normal |
		rcq_user::tex_flag::metal;
	mat_build_info->texfiles[rcq_user::tex_type::color] = "textures/rusted_iron/rustediron2_basecolor.png";
	mat_build_info->texfiles[rcq_user::tex_type::metal] = "textures/rusted_iron/rustediron2_metallic.png";
	mat_build_info->texfiles[rcq_user::tex_type::normal] = "textures/rusted_iron/rustediron2_normal.png";
	mat_build_info->texfiles[rcq_user::tex_type::roughness] = "textures/rusted_iron/rustediron2_roughness.png";

	//scuffed aluminium
	rcq_user::build_resource<rcq_user::resource::opaque_material>(&m_resources[resource::mat_scuffed_aluminum], &mat_build_info);
	mat_build_info->color= { 0.91f, 0.92f, 0.92f };
	mat_build_info->metal = 1.f;
	mat_build_info->tex_flags = rcq_user::tex_flag::roughness;
	mat_build_info->texfiles[rcq_user::tex_type::roughness] = "textures/scuffed_aluminium/Aluminum-Scuffed_roughness.png";

	//sand
	rcq_user::build_resource<rcq_user::resource::opaque_material>(&m_resources[resource::mat_sand], &mat_build_info);
	mat_build_info->tex_flags = rcq_user::tex_flag::color | rcq_user::tex_flag::roughness | rcq_user::tex_flag::normal;
	mat_build_info->metal = 0.f;
	mat_build_info->texfiles[rcq_user::tex_type::color] = "textures/sand1-ue/sand1-albedo.png";
	mat_build_info->texfiles[rcq_user::tex_type::normal] = "textures/sand1-ue/sand1-normal-ue.png";
	mat_build_info->texfiles[rcq_user::tex_type::roughness] = "textures/sand1-ue/sand1-roughness.png";

	//rocksand
	rcq_user::build_resource<rcq_user::resource::opaque_material>(&m_resources[resource::mat_rocksand], &mat_build_info);
	mat_build_info->tex_flags = rcq_user::tex_flag::color | rcq_user::tex_flag::roughness | rcq_user::tex_flag::normal |
		rcq_user::tex_flag::ao | rcq_user::tex_flag::height;
	mat_build_info->metal = 0.f;
	mat_build_info->height_scale = 0.3f;
	mat_build_info->texfiles[rcq_user::tex_type::color] = "textures/roughrock1-Unreal-Engine/roughrock1albedo3.png";
	mat_build_info->texfiles[rcq_user::tex_type::normal] = "textures/roughrock1-Unreal-Engine/roughrock1-normal4.png";
	mat_build_info->texfiles[rcq_user::tex_type::roughness] = "textures/roughrock1-Unreal-Engine/roughrock1roughness3.png";
	mat_build_info->texfiles[rcq_user::tex_type::ao] = "textures/roughrock1-Unreal-Engine/roughrock1_ao.png";
	mat_build_info->texfiles[rcq_user::tex_type::height] = "textures/roughrock1-Unreal-Engine/roughrock1-height.png";

	//grass
	rcq_user::build_resource<rcq_user::resource::opaque_material>(&m_resources[resource::mat_grass], &mat_build_info);
	mat_build_info->tex_flags = rcq_user::tex_flag::color | rcq_user::tex_flag::roughness | rcq_user::tex_flag::normal |
		rcq_user::tex_flag::ao | rcq_user::tex_flag::height;
	mat_build_info->metal = 0.f;
	mat_build_info->height_scale = 0.13f;
	mat_build_info->texfiles[rcq_user::tex_type::color] = "textures/grass1-Unreal-Engine2/grass1-albedo3.png";
	mat_build_info->texfiles[rcq_user::tex_type::normal] = "textures/grass1-Unreal-Engine2/grass1-normal2.png";
	mat_build_info->texfiles[rcq_user::tex_type::roughness] = "textures/grass1-Unreal-Engine2/grass1-rough.png";
	mat_build_info->texfiles[rcq_user::tex_type::ao] = "textures/grass1-Unreal-Engine2/grass1-ao.png";
	mat_build_info->texfiles[rcq_user::tex_type::height] = "textures/grass1-Unreal-Engine2/grass1-height.png";

	//rock
	rcq_user::build_resource<rcq_user::resource::opaque_material>(&m_resources[resource::mat_grass], &mat_build_info);
	mat_build_info->tex_flags = rcq_user::tex_flag::color | rcq_user::tex_flag::roughness | rcq_user::tex_flag::normal |
		rcq_user::tex_flag::ao | rcq_user::tex_flag::height;
	mat_build_info->metal = 0.f;
	mat_build_info->height_scale = 0.5f;
	mat_build_info->texfiles[rcq_user::tex_type::color] = "textures/slate-cliff-rock-ue4/slatecliffrock-albedo.png";
	mat_build_info->texfiles[rcq_user::tex_type::normal] = "textures/slate-cliff-rock-ue4/slatecliffrock_Normal.png";
	mat_build_info->texfiles[rcq_user::tex_type::roughness] = "textures/slate-cliff-rock-ue4/slatecliffrock_Roughness2.png";
	mat_build_info->texfiles[rcq_user::tex_type::ao] = "textures/slate-cliff-rock-ue4/slatecliffrock_Ambient_Occlusion.png";
	mat_build_info->texfiles[rcq_user::tex_type::height] = "textures/slate-cliff-rock-ue4/slatecliffrock_Height.png";

	//sky resource
	rcq_user::build_info<rcq_user::resource::sky>* sky_build_info;
	rcq_user::build_resource<rcq_user::resource::sky>(&m_resources[resource::sky], &sky_build_info);
	sky_build_info->filename = "textures/sky/try_12";
	sky_build_info->sky_image_size = glm::uvec3(32);
	sky_build_info->transmittance_image_size = glm::uvec2(512);

	//terrain resource
	rcq_user::build_info<rcq_user::resource::terrain>* terrain_build_info;
	rcq_user::build_resource<rcq_user::resource::terrain>(&m_resources[resource::terrain], &terrain_build_info);
	terrain_build_info->filename = "textures/terrain/t";
	terrain_build_info->level0_image_size = glm::uvec2(4096, 4096);
	terrain_build_info->level0_tile_size = glm::uvec2(512, 512);
	terrain_build_info->mip_level_count = 4;
	terrain_build_info->size_in_meters = glm::vec3(512.f, 10.f, 512.f);

	//water res
	m_wave_period = 10000.f;
	rcq_user::build_info<rcq_user::resource::water>* water_build_info;
	rcq_user::build_resource<rcq_user::resource::water>(&m_resources[resource::water], &water_build_info);
	water_build_info->filename = "textures/water/w.wat";
	water_build_info->A = 1e-4f;
	water_build_info->base_frequency = 2.f*PI / m_wave_period;
	water_build_info->grid_size_in_meters = glm::vec2(1024.f*0.04f);

	//create transforms
	rcq_user::build_info<rcq_user::resource::transform>* tr_build_info;

	//buddha
	rcq_user::build_resource<rcq_user::resource::transform>(&m_resources[resource::tr_buddha], &tr_build_info);
	tr_build_info->model = glm::mat4(1.f);
	tr_build_info->scale = glm::vec3(1.f);

	//floor
	rcq_user::build_resource<rcq_user::resource::transform>(&m_resources[resource::tr_floor], &tr_build_info);
	tr_build_info->model = glm::translate(glm::mat4(1.f), { 0.f, -1.f + 10.f, 0.f });
	tr_build_info->scale = glm::vec3(6.f);
	tr_build_info->tex_scale = glm::vec2(1.f);

	//self
	rcq_user::build_resource<rcq_user::resource::transform>(&m_resources[resource::tr_shelf], &tr_build_info);
	tr_build_info->model = glm::translate(glm::mat4(1.f), { 0.f, -0.4f + 10.f, -4.f });
	tr_build_info->scale = glm::vec3(0.015f);
	tr_build_info->tex_scale = glm::vec2(5.f);

	//rusted iron sphere
	rcq_user::build_resource<rcq_user::resource::transform>(&m_resources[resource::tr_rusted_iron_sphere], &tr_build_info);
	tr_build_info->model = glm::translate(glm::mat4(1.f), { -1., -0.75f + 10.f, -1.f });
	tr_build_info->scale = glm::vec3(0.2f);
	tr_build_info->scale = glm::vec3(1.f);

	//scuffed aluminium sphere
	rcq_user::build_resource<rcq_user::resource::transform>(&m_resources[resource::tr_scuffed_alu_sphere], &tr_build_info);
	tr_build_info->model = glm::translate(glm::mat4(1.f), { -3., -0.75f + 10.f, -3.f });
	tr_build_info->scale = glm::vec3(0.2f);
	tr_build_info->scale = glm::vec3(1.f);

	//dispatch resource builds
	rcq_user::dispatch_resource_builds();


	//create opaque objects
	m_opaque_objects.resize(opaque_object::count);
	//buddha
	rcq_user::add_opaque_object(m_resources[resource::mesh_buddha], m_resources[resource::mat_gold], m_resources[tr_buddha],
		&m_opaque_objects[opaque_object::buddha]);
	//self
	rcq_user::add_opaque_object(m_resources[resource::mesh_shelf], m_resources[resource::mat_bamboo_wood], m_resources[tr_shelf],
		&m_opaque_objects[opaque_object::shelf]);
	//floor
	rcq_user::add_opaque_object(m_resources[resource::mesh_plane], m_resources[resource::mat_oak_floor], m_resources[tr_floor],
		&m_opaque_objects[opaque_object::floor]);
	//rusted iron sphere
	rcq_user::add_opaque_object(m_resources[resource::mesh_sphere], m_resources[resource::mat_rusted_iron], 
		m_resources[tr_rusted_iron_sphere],&m_opaque_objects[opaque_object::rusted_iron_sphere]);
	//scuffed aluminium sphere
	rcq_user::add_opaque_object(m_resources[resource::mesh_sphere], m_resources[resource::mat_scuffed_aluminum], 
		m_resources[tr_scuffed_alu_sphere], &m_opaque_objects[opaque_object::scuffed_alu_sphere]);

	//sky
	rcq_user::set_sky(m_resources[resource::sky]);

	//terrain
	rcq_user::set_terrain(m_resources[resource::terrain]); //should add materials!!!

	//water
	rcq_user::set_water(m_resources[resource::water]);	

	//set camera
	m_camera.pos = glm::vec3(0.f, 10.f, 0.f);
	m_camera.look_dir = glm::normalize(glm::vec3(-2.f));
	m_camera.proj = glm::perspective(glm::radians(45.f), m_window_size.x / m_window_size.y, 0.1f, 500.f);
	m_camera.proj[1][1] *= (-1);
	m_camera.view = glm::lookAt(glm::vec3(2.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));

	m_render_settings.ambient_irradiance = glm::vec3(0.2f);
	m_render_settings.far = 500.f;
	m_render_settings.irradiance = glm::vec3(10.f);
	m_render_settings.light_dir = glm::normalize(glm::vec3(0.1f, -1.f, 0.f));
	m_render_settings.near = 0.1f;
	m_render_settings.pos = m_camera.pos;
	m_render_settings.proj = m_camera.proj;
	m_render_settings.view = m_camera.view;
	m_render_settings.wind = { 31.f, 0.f};
	m_render_settings.time = 0.f;


}

void scene::update(float dt)
{
	static float elapsed_time = 0.f;
	elapsed_time += dt;


	/*for (int i = 0; i < buddha_count; ++i)
	{
		m_trs[i].data.model = glm::rotate(m_trs[i].data.model, glm::radians(90.f*dt), glm::vec3(0.f, 1.f, 0.f));
		rcq::engine::instance()->cmd_update_transform(m_trs[i].id, m_trs[i].data);
	}*/
	update_settings(dt);

	rcq_user::set_render_settings(m_render_settings);
	rcq_user::render();

	//std::cout << "wind: " << m_render_settings.wind.x << ' ' <<m_render_settings.wind.y << '\n';
}

void scene::update_settings(float dt)
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
		move = glm::normalize(move);

		if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT))
			move *= 20.f;

		m_camera.pos += dt*glm::mat3(m_camera.look_dir, glm::vec3(0.f, -1.f, 0.f), glm::vec3(-m_camera.look_dir.z, 0.f, m_camera.look_dir.x))
			*move;
	}

	m_camera.view = glm::lookAt(m_camera.pos, m_camera.pos + m_camera.look_dir, glm::vec3(0.f, 1.f, 0.f));

	m_render_settings.pos = m_camera.pos;
	m_render_settings.view = m_camera.view;
	m_render_settings.time += dt;
	while (m_render_settings.time > m_wave_period)
		m_render_settings.time -= m_wave_period;

	//light 
	float theta = 0.f;
	float phi = 0.f;
	float scale = 0.2f;

	if (glfwGetKey(m_window, GLFW_KEY_I))
		phi += 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_K))
		phi -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_J))
		theta += 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_L))
		theta -= 1.f;

	glm::mat4 rot = glm::rotate(scale*dt*theta, glm::vec3(0.f, 1.f, 0.f));
	rot = glm::rotate(rot, scale*dt*phi, glm::vec3(1.f, 0.f, 0.f));
	m_render_settings.light_dir = static_cast<glm::mat3>(rot)*m_render_settings.light_dir;

	//wind
	theta = 0.f;
	float speed = 0.f;

	if (glfwGetKey(m_window, GLFW_KEY_N))
		speed += 5.f;
	if (glfwGetKey(m_window, GLFW_KEY_M))
		speed -= 5.f;
	if (glfwGetKey(m_window, GLFW_KEY_V))
		theta += 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_B))
		theta -= 1.f;

	m_render_settings.wind = (glm::length(m_render_settings.wind) + dt*speed)*glm::normalize(m_render_settings.wind);

	glm::mat2 rot2d(cosf(theta), sinf(theta), -sinf(theta), cosf(theta));
	m_render_settings.wind = rot2d*m_render_settings.wind;
}