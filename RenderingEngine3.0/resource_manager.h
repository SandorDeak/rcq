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

		void process_build_package(build_package&& package);
		void push_destroy_package(std::unique_ptr<destroy_package>&& package)
		{
			std::lock_guard<std::mutex> lock(m_destroy_p_queue_mutex);
			m_destroy_p_queue.push(std::move(package));
		}


		template<RESOURCE_TYPE res_type>
		const auto& get(unique_id id);


	private:

		resource_manager(const base_info& info);
		void destroy_texture(const texture& tex);

		texture create_depth(int width, int height, int layer_count);
		void destroy_depth(RENDER_PASS);

		

		/*template<size_t res_type>
		void create_destroy_tasks_impl(std::vector<unique_id>& destroy_ids);

		template<size_t... types, typename DestroyPackage>
		void create_destroy_tasks(DestroyPackage&& p, std::index_sequence<types...>);*/
	

		static resource_manager* m_instance;

		const base_info m_base;

		//resources
		std::tuple<
			std::map<unique_id, material>,
			std::map<unique_id, mesh>,
			std::map<unique_id, transform>
		> m_resources_ready;
		std::mutex m_resources_ready_mutex;

		std::tuple<
			std::map<unique_id, std::future<material>>,
			std::map<unique_id, std::future<mesh>>,
			std::map<unique_id, std::future<transform>>
		> m_resources_proc;
		std::mutex m_resources_proc_mutex;

		std::map<RENDER_PASS, texture> m_depth_ready;
		std::map<RENDER_PASS, std::future<texture>> m_depth_proc;

		template<size_t... res_types>
		void check_resource_leak(std::index_sequence<res_types...>)
		{
			auto l = { (check_resource_leak_impl<res_types>(),0)... };
		}

		template<size_t res_type>
		void check_resource_leak_impl()
		{
			if (!std::get<res_type>(m_resources_ready).empty())
				throw std::runtime_error("resource leak of resource type " + std::to_string(res_type)+ 
					". used resource is not destroyed");
			if (!std::get<res_type>(m_resources_proc).empty())
				throw std::runtime_error("resource leak of resource type " + std::to_string(res_type) +
					". resource is build but never used and deleted");
		}


		//build thread
		std::thread m_build_thread;
		void build_loop();
		std::queue<std::unique_ptr<build_task_package>> m_build_task_p_queue;
		std::mutex m_build_task_p_queue_mutex;
		std::unique_ptr<build_task_package> m_current_build_task_p;
		bool m_should_end_build;

		template<typename TaskTuple, size_t... types>
		void do_build_tasks(TaskTuple&& p, std::index_sequence<types...>);

		template<size_t res_type, typename Id, typename... Infos>
		void create_build_tasks_impl(std::vector<std::tuple<Id, Infos...>>& build_infos);


		template<size_t... res_types, typename BuildPackage>
		inline void create_build_tasks(BuildPackage&& p, std::index_sequence<res_types...>);

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

		//destroy thread
		std::thread m_destroy_thread;
		void destroy_loop();
		std::queue<std::unique_ptr<destroy_package>> m_destroy_p_queue;
		std::mutex m_destroy_p_queue_mutex;
		destroy_ids m_pending_destroys;
		bool m_should_end_destroy;
		std::tuple<
			std::vector<material>,
			std::vector<mesh>,
			std::vector<transform>
		> m_destroyables;

		template<size_t... res_types>
		inline void check_pending_destroys(std::index_sequence<res_types...>);


		template<size_t res_type>
		inline void check_pending_destroys_impl();

		template<size_t... res_types>
		inline void process_destroy_package(const destroy_ids& package, std::index_sequence<res_types...>);

		template<size_t res_type>
		inline void process_destroy_package_element(const std::vector<unique_id>& ids);

		template<size_t... res_types>
		void destroy_destroyables(std::index_sequence<res_types...>);

		template<typename ResourceType>
		void destroy(ResourceType&& res) {};
		template<>
		void destroy(mesh&& _mesh);
		template<>
		void destroy(material&& _mat);
		void destroy(transform&& _tr);
	};


	//implementations
	template<RESOURCE_TYPE res_type>
	const auto& resource_manager::get(unique_id id)
	{
		static auto& res_ready = std::get<res_type>(m_resources_ready);
		static auto& res_proc = std::get<res_type>(m_resources_proc);
		{
			std::lock_guard<std::mutex> lock_ready(m_resources_ready_mutex);
			if (auto itr = res_ready.find(id); itr != res_ready.end())
				return itr->second;
		}

		std::lock_guard<std::mutex> lock_proc(m_resources_proc_mutex);
		if (auto itp = res_proc.find(id); itp != res_proc.end())
		{
			auto ready_resource = itp->second.get();
			std::lock_guard<std::mutex> lock_ready(m_resources_ready_mutex);
			auto[itr, insert] = res_ready.insert_or_assign(id, itp->second.get());
			if (!insert)
				throw std::runtime_error("id conflict!");
			res_proc.erase(itp);
			return itr->second;
		}

		throw std::runtime_error("invalid id!");
	}

	template<size_t res_type, typename Id, typename... Infos>
	void resource_manager::create_build_tasks_impl(std::vector<std::tuple<Id, Infos...>>& build_infos)
	{
		auto& task_vector = std::get<res_type>(*m_current_build_task_p.get());
		task_vector.reserve(build_infos.size());

		using TaskType = std::remove_reference_t<decltype(*task_vector.data())>;

		for (auto& build_info : build_infos)
		{
			TaskType task(std::bind(&resource_manager::build<res_type, Infos...>, this,
				std::get<Infos>(build_info)...));
			
			{
				std::lock_guard<std::mutex> lock(m_resources_proc_mutex);
				bool insert = std::get<res_type>(m_resources_proc).insert_or_assign(std::get<Id>(build_info), task.get_future()).second;
				if (!insert)
					throw std::runtime_error("unique id conflict!");
			}
			task_vector.push_back(std::move(task));
		}
	}

	template<size_t... res_types, typename BuildPackage>
	void resource_manager::create_build_tasks(BuildPackage&& p, std::index_sequence<res_types...>)
	{
		auto l = { (create_build_tasks_impl<res_types>(std::get<res_types>(p)), 0)... };
	}

	/*template<size_t res_type>
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
	}*/

	template<typename TaskTuple, size_t... types>
	void resource_manager::do_build_tasks(TaskTuple&& p, std::index_sequence<types...>)
	{
		auto l = { ([](auto&& task_vector) { for (auto& task : task_vector) task(); }(std::get<types>(p)), 0)... };
	}

	template<size_t... res_types>
	inline void resource_manager::check_pending_destroys(std::index_sequence<res_types...>)
	{
		auto l = { (check_pending_destroys_impl<res_types>() ,0)... };
	}

	template<size_t res_type>
	inline void resource_manager::check_pending_destroys_impl()
	{
		static auto& resources_ready = std::get<res_type>(m_resources_ready);
		static auto& pending_destroys = std::get<res_type>(m_pending_destroys);
		pending_destroys.erase(std::remove_if(pending_destroys.begin(), pending_destroys.end(), [this](unique_id id)
		{
			if (auto it = resources_ready.find(id); it != resources_ready.end())
			{
				std::get<res_type>(m_destroyables).push_back(std::move(it->second));
				resources_ready.erase(it);
				return true;
			}
			return false;
		}), pending_destroys.end());
	}

	template<size_t... res_types>
	inline void resource_manager::process_destroy_package(const destroy_ids& package, std::index_sequence<res_types...>)
	{
		auto l = { (process_destroy_package_element<res_types>(std::get<res_types>(package)), 0)... };
	}

	template<size_t res_type>
	inline void resource_manager::process_destroy_package_element(const std::vector<unique_id>& ids)
	{
		static auto& res_ready = std::get<res_type>(m_resources_ready);
		for (unique_id id : ids)
		{
			if (auto it = res_ready.find(id); it != res_ready.end())
			{
				std::get<res_type>(m_destroyables).push_back(std::move(it->second));
				res_ready.erase(it);
			}
			else
			{
				std::get<res_type>(m_pending_destroys).push_back(id);
			}
		}
	}

	template<size_t... res_types>
	void resource_manager::destroy_destroyables(std::index_sequence<res_types...>)
	{
		auto l = { ([this](auto&& destroyable_vect)
		{
			for (const auto& destroyable : destroyable_vect)
				destroy(destroyable);
			destroyable_vect.clear();
		}(std::get<res_types>(m_destroyables)), 0)... };
	}
}