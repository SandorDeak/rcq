#pragma once

#include "foundation.h"

namespace rcq
{

	class engine
	{
	public:
		engine(const engine&) = delete;
		engine(engine&&) = delete;
		~engine();

		static void init();
		static void destroy();
		static engine* instance() { return m_instance; }

		GLFWwindow* get_window() { return m_base.window; }
		glm::vec2 get_window_size() { return m_window_size; }

		template<RESOURCE_TYPE type, typename... Ts>
		void cmd_build(Ts&&... args)
		{
			if (!m_build_p)
				m_build_p.reset(new build_package);
			std::get<type>(*m_build_p.get()).emplace_back(std::forward<Ts>(args)...);
		}

		template<RESOURCE_TYPE type>
		void cmd_destroy(unique_id id)
		{
			if (!m_destroy_p)
				m_destroy_p.reset(new destroy_package);

			std::get<type>(m_destroy_p->ids).push_back(id);
		}

		void cmd_build_renderable(unique_id id, unique_id tr_id, unique_id mesh_id, unique_id mat_id, RENDERABLE_TYPE type, 
			LIFE_EXPECTANCY life_exp)
		{
			if (!m_core_p)
				m_core_p.reset(new core_package);
			m_core_p->build_renderable[type*LIFE_EXPECTANCY_COUNT+life_exp].emplace_back(id, tr_id, mesh_id, mat_id);
		}

		void cmd_destroy_renderable(unique_id uid, RENDERABLE_TYPE type, LIFE_EXPECTANCY life_exp)
		{
			if (!m_core_p)
				m_core_p.reset(new core_package);
			m_core_p->destroy_renderable[type*LIFE_EXPECTANCY_COUNT + life_exp].push_back(uid);
		}

		void cmd_update_transform(unique_id id, const transform_data& tr_data)
		{
			if (!m_core_p)
				m_core_p.reset(new core_package);
			m_core_p->update_tr.emplace_back(id, tr_data);
		}

		void cmd_render(const glm::mat4& view, const std::optional<update_proj>& proj_info)
		{
			if (!m_core_p)
				m_core_p.reset(new core_package);
			m_core_p->render = true;
			m_core_p->view = view;
			m_core_p->proj_info = proj_info;
		}

		void cmd_dispatch();

	private:
		engine();
		static engine* m_instance;
		glm::vec2 m_window_size;

		std::unique_ptr<core_package> m_core_p;
		std::unique_ptr<build_package> m_build_p;
		std::unique_ptr<destroy_package> m_destroy_p;

		base_info m_base;
	};
}