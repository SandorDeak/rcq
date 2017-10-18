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

		void process_build_package(build_package&& package);
		void push_destroy_package(std::unique_ptr<destroy_package>&& package)
		{
			std::lock_guard<std::mutex> lock(m_destroy_p_queue_mutex);
			m_destroy_p_queue.push(std::move(package));
		}


		template<size_t res_type>
		const auto& get_res(unique_id id);

		void update_tr(const std::vector<update_tr_info>& trs);
		VkDescriptorSetLayout get_dsl(DESCRIPTOR_SET_LAYOUT_TYPE type) { return m_dsls[type]; }

	private:
		resource_manager(const base_info& info);
		
		void create_samplers();
		void create_descriptor_set_layouts();
		void create_staging_buffers();
		void create_command_pool();


		template<DESCRIPTOR_SET_LAYOUT_TYPE dsl_type>
		inline void extend_descriptor_pool_pool();

		template<size_t... render_passes>
		inline void wait_for_finish(std::index_sequence<render_passes...>);

		static resource_manager* m_instance;

		const base_info m_base;

		VkCommandPool m_cp_build;
		VkCommandPool m_cp_update;

		//resources
		VkBuffer m_single_cell_sb; //used by build thread
		VkDeviceMemory m_single_cell_sb_mem;
		char* m_single_cell_sb_data;

		static const uint32_t STAGING_BUFFER_CELL_COUNT = 128;
		VkBuffer m_sb; //used by core thread, size=
		VkDeviceMemory m_sb_mem;
		char* m_sb_data;

		std::array<VkSampler, SAMPLER_TYPE_COUNT> m_samplers;
		std::array<VkDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_TYPE_COUNT> m_dsls;

		static const uint32_t DESCRIPTOR_POOL_SIZE = 64;
		std::array<descriptor_pool_pool, DESCRIPTOR_SET_LAYOUT_TYPE_COUNT> m_dpps;
		


		std::tuple<
			std::map<unique_id, material_opaque>,
			std::map<unique_id, material_em>,
			std::map<unique_id, light_omni>,
			std::map<unique_id, skybox>,
			std::map<unique_id, mesh>,
			std::map<unique_id, transform>,
			std::map<unique_id, memory>
		> m_resources_ready;
		std::mutex m_resources_ready_mutex;

		std::tuple<
			std::map<unique_id, std::future<material_opaque>>,
			std::map<unique_id, std::future<material_em>>,
			std::map<unique_id, std::future<light_omni>>,
			std::map<unique_id, std::future<skybox>>,
			std::map<unique_id, std::future<mesh>>,
			std::map<unique_id, std::future<transform>>,
			std::map<unique_id, std::future<memory>>
		> m_resources_proc;
		std::mutex m_resources_proc_mutex;


		template<size_t... res_types>
		inline void check_resource_leak(std::index_sequence<res_types...>)
		{
			auto l = { (check_resource_leak_impl<res_types>(),0)... };
		}

		template<size_t res_type>
		inline void check_resource_leak_impl()
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
		inline void do_build_tasks(TaskTuple&& p, std::index_sequence<types...>);

		template<size_t res_type, typename VectOfTuple, size_t IdIndex, size_t... indices>
		inline void create_build_tasks_impl(VectOfTuple&& build_infos, std::index_sequence<IdIndex, indices...>);


		template<size_t... res_types>
		inline void create_build_tasks(build_package&& p, std::index_sequence<res_types...>);

		template<size_t res_type>
		typename std::enable_if_t<std::is_same_v<mesh, typename resource_typename<res_type>::type>, mesh>
			build(const std::string& filename, bool calc_tb);

		template<size_t res_type>
		typename std::enable_if_t<std::is_same_v<material_opaque, typename resource_typename<res_type>::type>, material_opaque>
		 build(const material_opaque_data& data, const texfiles& files);

		template<size_t res_type>
		typename std::enable_if_t<std::is_same_v<transform, typename resource_typename<res_type>::type>, transform>
			build(const transform_data& data, USAGE usage);

		template<size_t res_type>
		typename std::enable_if_t<std::is_same_v<light_omni, typename resource_typename<res_type>::type>, light_omni>
			build(const light_omni_data& data, USAGE usage);

		template<size_t res_type>
		typename std::enable_if_t<std::is_same_v<memory, typename resource_typename<res_type>::type>, memory>
			build(const std::vector<VkMemoryAllocateInfo>& requirements);

		template<size_t res_type>
		typename std::enable_if_t<std::is_same_v<skybox, typename resource_typename<res_type>::type>, skybox>
			build(const std::string& filename);

		template<size_t res_type>
		typename std::enable_if_t<std::is_same_v<material_em, typename resource_typename<res_type>::type>, material_em>
			build(const std::string& x) { return material_em(); }


		texture load_texture(const std::string& filename);


		//destroy thread
		std::thread m_destroy_thread;
		void destroy_loop();
		std::queue<std::unique_ptr<destroy_package>> m_destroy_p_queue;
		std::mutex m_destroy_p_queue_mutex;
		destroy_ids m_pending_destroys;
		bool m_should_end_destroy;
		std::tuple<
			std::vector<material_opaque>,
			std::vector<material_em>,
			std::vector<light_omni>,
			std::vector<skybox>,
			std::vector<mesh>,
			std::vector<transform>,
			std::vector<memory>
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
		inline void destroy_destroyables(std::index_sequence<res_types...>);

	
		void destroy(mesh&& _mesh);
		void destroy(material_opaque&& _mat);
		void destroy(transform&& _tr);
		void destroy(light_omni&& _light);
		void destroy(memory&& _memory);
		void destroy(skybox&& _sb);
		void destroy(material_em&& _mat) {}

	};


	//implementations
	template<size_t res_type>
	const auto& resource_manager::get_res(unique_id id)
	{
		auto& res_ready = std::get<res_type>(m_resources_ready);
		auto& res_proc = std::get<res_type>(m_resources_proc);
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
			auto[itr, insert] = res_ready.insert_or_assign(id, ready_resource);
			if (!insert)
				throw std::runtime_error("id conflict!");
			res_proc.erase(itp);
			return itr->second;
		}

		throw std::runtime_error("invalid id!");
	}

	template<size_t res_type, typename VectOfTuple, size_t IdIndex, size_t... indices>
	void resource_manager::create_build_tasks_impl(VectOfTuple&& build_infos, std::index_sequence<IdIndex, indices...>)
	{
		auto& task_vector = std::get<res_type>(*m_current_build_task_p.get());
		task_vector.reserve(build_infos.size());
		//std::map<unique_id, std::future<resource_typename<res_type>::type>> m;
		//std::remove_reference_t<decltype(std::get<res_type>(m_resources_proc))> m;
		std::tuple_element_t<res_type, decltype(m_resources_proc)> m;
		using TaskType = std::remove_reference_t<decltype(*task_vector.data())>;

		for (auto& build_info : build_infos)
		{
			TaskType task([t=std::make_tuple(this, std::move(std::get<indices>(build_info))...)]() 
			{
				return std::apply(&resource_manager::build<res_type>, std::move(t));
			});
			m.insert_or_assign(std::get<IdIndex>(build_info), task.get_future());
			task_vector.push_back(std::move(task));
		}
		std::lock_guard<std::mutex> lock(m_resources_proc_mutex);
		for (auto& node : m)
		{
			bool insert=std::get<res_type>(m_resources_proc).insert_or_assign(node.first, std::move(node.second)).second; //should use extract when it's available
			if (!insert)
				throw std::runtime_error("id conflict!");
		}
	}

	template<size_t... res_types>
	void resource_manager::create_build_tasks(build_package&& p, std::index_sequence<res_types...>)
	{
		auto l = { (create_build_tasks_impl<res_types>(std::get<res_types>(p), 
			std::make_index_sequence<std::tuple_size_v<std::tuple_element_t<res_types, build_package>::value_type>>()), 0)... };

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
		auto& pending_destroys = std::get<res_type>(m_pending_destroys);

		pending_destroys.erase(std::remove_if(pending_destroys.begin(), pending_destroys.end(), [this](unique_id id)
		{
			if (auto it = std::get<res_type>(m_resources_ready).find(id); it != std::get<res_type>(m_resources_ready).end())
			{
				std::get<res_type>(m_destroyables).push_back(std::move(it->second));
				std::get<res_type>(m_resources_ready).erase(it);
				return true;
			}
			if (auto it = std::get<res_type>(m_resources_proc).find(id); it != std::get<res_type>(m_resources_proc).end())
			{
				std::get<res_type>(m_destroyables).push_back(std::move(it->second.get()));
				std::get<res_type>(m_resources_proc).erase(it);
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
			for (auto& destroyable : destroyable_vect)
				destroy(std::move(destroyable));
			destroyable_vect.clear();
		}(std::get<res_types>(m_destroyables)), 0)... };
	}
}