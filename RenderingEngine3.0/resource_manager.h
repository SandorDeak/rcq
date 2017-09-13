#pragma once

#include "foundation.h"

namespace rcq
{
	class resource_manager
	{
	public:
		resource_manager(const resource_manager&) = delete;
		resource_manager(resource_manager&&) = delete;

		~resource_manager();

		static void init(const base_info& info);
		static void destroy();
		static resource_manager* instance() { return m_instance; }

		texture create_depth_async(RENDER_PASS pass, int width, int height, int layer_count);
		void destroy_depth_async(RENDER_PASS);

		void process_package_async(resource_manager_package&& package);

		template<RESOURCE_TYPE res_type>
		const auto& get(unique_id id);


	private:

		resource_manager(const base_info& info);
		void destroy_texture(const texture& tex);

		template<size_t res_type, typename... Ts>
		typename resource_typename<res_type>::type build(Ts... args)
		{
			return resource_typename<res_type>::type();
		}

		template<>
		mesh build<RESOURCE_TYPE_MESH>(std::string filename, bool calc_tb);

		template<>
		material build<RESOURCE_TYPE_MAT>(material_data data, texfiles files, MAT_TYPE type);

		template<>
		transform build<RESOURCE_TYPE_TR>(transform_data data, USAGE usage);



		template<size_t type>
		void destroy(unique_id id);

		texture create_depth(int width, int height, int layer_count);
		void destroy_depth(RENDER_PASS);

		template<size_t res_type, typename Id, typename... Infos>
		void create_build_tasks_impl(std::vector<std::tuple<Id, Infos...>>& build_infos);


		template<size_t... types, typename BuildPackage>
		void create_build_tasks(BuildPackage&& p, std::index_sequence<types...>);

		template<size_t res_type>
		void create_destroy_tasks_impl(std::vector<unique_id>& destroy_ids);

		template<size_t... types, typename DestroyPackage>
		void create_destroy_tasks(DestroyPackage&& p, std::index_sequence<types...>);

		template<typename TaskTuple, size_t... types>
		void do_tasks_impl(TaskTuple&& p, std::index_sequence<types...>);

		void work();
		void do_tasks(resource_manager_task_package& package);

		static resource_manager* m_instance;

		std::thread m_working_thread;
		bool m_should_end;

		const base_info m_base;

		std::tuple<
			std::map<unique_id, material>,
			std::map<unique_id, mesh>,
			std::map<unique_id, transform>
		> m_resources_ready;

		std::tuple<
			std::map<unique_id, std::future<material>>,
			std::map<unique_id, std::future<mesh>>,
			std::map<unique_id, std::future<transform>>
		> m_resources_proc;

		std::map<RENDER_PASS, texture> m_depth_ready;
		std::map<RENDER_PASS, std::future<texture>> m_depth_proc;

		std::queue<std::unique_ptr<resource_manager_task_package>> m_task_p_queue;
		std::mutex m_task_p_queue_mutex;

		std::unique_ptr<resource_manager_task_package> m_current_task_p;

	};


	//implementations
	template<RESOURCE_TYPE res_type>
	const auto& resource_manager::get(unique_id id)
	{
		auto& res_ready = std::get<res_type>(m_resources_ready);
		auto& res_proc = std::get<res_type>(m_resources_proc);

		if (auto itr = res_ready.find(id); itr != res_ready.end())
			return itr->second;
		else if (auto itp = res_proc.find(id); itp != res_proc.end())
		{
			auto[itr2, insert] = res_ready.insert_or_assign(id, itp->second.get());
			if (!insert)
				throw std::runtime_error("id conflict!");
			res_proc.erase(itp);
			return itr2->second;
		}
		else
		{
			throw std::runtime_error("invalid id!");
		}
	}

	template<size_t res_type, typename Id, typename... Infos>
	void resource_manager::create_build_tasks_impl(std::vector<std::tuple<Id, Infos...>>& build_infos)
	{
		auto& task_vector = std::get<res_type>(m_current_task_p->build_task_p);
		task_vector.reserve(build_infos.size());

		using TaskType = std::remove_reference_t<decltype(*task_vector.data())>;

		for (auto& build_info : build_infos)
		{
			TaskType task(std::bind(&resource_manager::build<res_type, Infos...>, this,
				std::get<Infos>(build_info)...));

			bool insert = std::get<res_type>(m_resources_proc).insert_or_assign(std::get<Id>(build_info), task.get_future()).second;
			if (!insert)
			{
				throw std::runtime_error("unique id conflict!");
			}
			task_vector.push_back(std::move(task));
		}
	}

	template<size_t... types, typename BuildPackage>
	void resource_manager::create_build_tasks(BuildPackage&& p, std::index_sequence<types...>)
	{
		auto l = { (create_build_tasks_impl<types>(std::get<types>(p)), 0)... };
	}

	template<size_t res_type>
	void resource_manager::create_destroy_tasks_impl(std::vector<unique_id>& destroy_ids)
	{
		auto& task_vector = std::get<res_type>(m_current_task_p->destroy_task_p);
		task_vector.reserve(destroy_ids.size());
		for (auto destroy_id : destroy_ids)
			task_vector.emplace_back(std::bind(&resource_manager::destroy<res_type>,
				this, destroy_id));
	}

	template<size_t... types, typename DestroyPackage>
	void resource_manager::create_destroy_tasks(DestroyPackage&& p, std::index_sequence<types...>)
	{
		auto l = { (create_destroy_tasks_impl<types>(std::get<types>(p)), 0)... };
	}

	template<typename TaskTuple, size_t... types>
	void resource_manager::do_tasks_impl(TaskTuple&& p, std::index_sequence<types...>)
	{
		auto l = { ([](auto&& task_vector) { for (auto& task : task_vector) task(); }(std::get<types>(p)), 0)... };
	}
}