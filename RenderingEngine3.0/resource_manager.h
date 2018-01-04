#pragma once

#include "queue.h"

#include "dp_pool.h"
#include "resources.h"
#include "base_info.h"

#include "pool_host_memory.h"
#include "monotonic_buffer_device_memory.h"
#include "vk_allocator.h"
#include "vk_memory.h"
#include "freelist_device_memory.h"
#include "freelist_host_memory.h"

#include "enum_dsl_type.h"

#include <mutex>

namespace rcq
{
	class resource_manager
	{

	public:
		static void init(const base_info& info);
		static void destroy();
		static resource_manager* instance() { return m_instance; }

		template<uint32_t res_type>
		void build_resource(base_resource** base_res, typename resource<res_type>::build_info** build_info)
		{
			*base_res = reinterpret_cast<base_resource*>(m_resource_pool.allocate(sizeof(base_resource), alignof(base_resource)));
			base_resource_build_info* raw_build_info = m_build_queue.create_back();
			raw_build_info->resource_type = res_type;
			raw_build_info->base_res = *base_res;
			*build_info = reinterpret_cast<typename resource<res_type>::build_info*>(raw_build_info->data);
			(*base_res)->res_type = res_type;
			(*base_res)->ready_bit = false;
		}

		void destroy_resource(base_resource* res)
		{
			*m_destroy_queue.create_back() = res;
		}

		void dispach_builds()
		{
			bool should_notify;
			{
				std::unique_lock<std::mutex> lock(m_build_queue_mutex);
				should_notify = m_build_queue.empty();
				m_build_queue.commit();
			}
			if (should_notify)
				m_build_queue_cv.notify_one();
		}

		void dispatch_destroys()
		{
			bool should_notify;
			{
				std::unique_lock<std::mutex> lock(m_destroy_queue_mutex);
				should_notify = m_destroy_queue.empty();
				m_destroy_queue.commit();
			}
			if (should_notify)
				m_destroy_queue_cv.notify_one();
		}

		VkDescriptorSetLayout get_dsl(DSL_TYPE dsl_type)
		{
			return m_dsls[dsl_type];
		}

	private:
		//create functions
		void create_dsls();
		void create_dp_pools();
		void create_command_pool();
		void create_memory_resources_and_containers();

		//thread loops
		void build_loop();
		void destroy_loop();

		//resource build, destroy functions
		template<uint32_t res_type> void build(base_resource* res, const char* build_info);
		template<uint32_t res_type> void destroy(base_resource* res);

		//ctor, dtor, singleton pattern
		resource_manager(const base_info& base);
		resource_manager(const resource_manager&) = delete;
		resource_manager(resource_manager&&) = delete;
		resource_manager& operator=(const resource_manager&) = delete;
		resource_manager& operator=(resource_manager&&) = delete;
		~resource_manager();
		static resource_manager* m_instance;

		//base info
		const base_info& m_base;

		//memory resource
		freelist_host_memory m_host_memory;
		vk_allocator m_vk_alloc;
		pool_host_memory m_resource_pool;
		vk_memory m_vk_mappable_memory;
		monotonic_buffer_device_memory m_mappable_memory;	
		vk_memory m_vk_device_memory;
		freelist_device_memory m_device_memory;

		//threads
		std::thread m_build_thread;
		std::thread m_destroy_thread;

		//atomic bits
		std::atomic_bool m_should_end_build;
		std::atomic_bool m_should_end_destroy;
		std::atomic_bool m_resource_pool_sync_bit;

		//queues
		queue<base_resource_build_info> m_build_queue;
		queue<base_resource*> m_destroy_queue;

		//mutexes
		std::mutex m_build_queue_mutex;
		std::mutex m_destroy_queue_mutex;

		//condition variables
		std::condition_variable m_build_queue_cv;
		std::condition_variable m_destroy_queue_cv;

		//pools
		dp_pool m_dp_pools[DSL_TYPE_COUNT];

		//descriptor set layouts
		VkDescriptorSetLayout m_dsls[DSL_TYPE_COUNT];

		//others
		VkBuffer m_staging_buffer;
		VkFence m_build_f;
		VkCommandPool m_build_cp;
		VkCommandBuffer m_build_cb;

		//helper functions
		void begin_build_cb();
		void end_build_cb(const VkSemaphore* wait_semaphores=nullptr, const VkPipelineStageFlags* wait_flags=nullptr, 
			uint32_t wait_count=0);
		void wait_for_build_fence();
	};
}
