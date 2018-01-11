#pragma once

#include "rcq_engine.h"

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

	struct resource
	{
		enum
		{
			//mesh_buddha,
			mesh_plane,
			mesh_shelf,
			mesh_sphere,
			mat_gold,
			mat_bamboo_wood,
			mat_oak_floor,
			mat_rusted_iron,
			mat_scuffed_aluminum,
			mat_sand,
			mat_rocksand,
			mat_grass,
			mat_rock,
			sky,
			terrain,
			water,
			tr_buddha,
			tr_floor,
			tr_shelf,
			tr_rusted_iron_sphere,
			tr_scuffed_alu_sphere,
			count
		};
	};

	struct opaque_object
	{
		enum
		{
			buddha,
			shelf,
			floor,
			rusted_iron_sphere,
			scuffed_alu_sphere,
			count
		};
	};

	struct camera
	{
		glm::vec3 pos;
		glm::vec3 look_dir;
		glm::mat4 proj;
		glm::mat4 view;

	};

	void build();
	void update_settings(float dt);

	glm::vec2 m_window_size;

	rcq_user::render_settings m_render_settings;
	camera m_camera;

	std::vector<rcq_user::resource_handle> m_resources;
	std::vector<rcq_user::renderable_handle> m_opaque_objects;
	rcq_user::renderable_handle m_terrain;
	rcq_user::renderable_handle m_sky;
	rcq_user::renderable_handle m_ocean;

	float m_wave_period;

	GLFWwindow* m_window;
};