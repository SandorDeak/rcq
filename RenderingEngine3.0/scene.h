#pragma once

#include "entity.h"

class scene
{
public:
	scene(GLFWwindow* window, const glm::vec2& window_size);
	~scene();
	scene(const scene&) = delete;
	scene(scene&&) = delete;
	scene& operator=(const scene&) = delete;
	scene& operator=(scene&&) = delete;

	void update(float dt);
private:
	void build();
	void update_camera(float dt);

	glm::vec2 m_window_size;

	camera m_camera;
	std::vector<entity> m_entities;

	std::vector<material_opaque> m_mats;
	std::vector<mesh> m_meshes;
	std::vector<transform> m_trs;
	std::vector<light_omni> m_light_omni;
	skybox m_skybox;

	rcq::render_settings m_render_settings;

	GLFWwindow* m_window;
};

