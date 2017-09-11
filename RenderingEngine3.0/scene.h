#pragma once

#include "entity.h"

class scene
{
public:
	scene();
	~scene();
	scene(const scene&) = delete;
	scene(scene&&) = delete;
	scene& operator=(const scene&) = delete;
	scene& operator=(scene&&) = delete;

	void update(float dt);
private:
	void build();
	void init_camera();

	camera m_camera;
	std::vector<entity> m_entities;

	std::map<size_t, material> m_materials;
	std::map<size_t, mesh> m_meshes;
};

