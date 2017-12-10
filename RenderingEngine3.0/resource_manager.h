#pragma once


#include "foundation2.h"
#include "queue.h"
#include "stack.h"
#include "array.h"
#include "pool_host_memory.h"
#include "monotonic_buffer_device_memory.h"
#include "vk_allocator.h"
#include "vk_memory.h"
#include "freelist_device_memory.h"
#include "freelist_host_memory.h"
#include <atomic>
#include <mutex>

namespace rcq
{
	class resource_manager
	{

	public:
		resource_manager(const resource_manager&) = delete;
		resource_manager(resource_manager&&) = delete;

		~resource_manager();

		static void init(const base_info& info, size_t place);
		static void destroy();
		static resource_manager* instance() { return m_instance; }

		template<uint32_t res_type>
		struct resource_user_info
		{
			resource<res_type>::build_info* build_info;
			base_resource* resource;
		};

		template<uint32_t res_type>
		resource_user_info<res_type> create_resource()
		{
			resource_user_info<res_type> ret;
			ret.resource = reinterpret_cast<base_resource*>(m_resource_pool->allocate(sizeof(base_resource), alignof(base_resource)));
			base_resource_build_info* raw_build_info = m_build_queue.create_back();
			raw_build_info->resource_type = res_type;
			raw_build_info->base_res = ret.resource;
			ret.build_info = reinterpret_cast<resource<res_type>::build_info*>(raw_build_info->data);
			ret.resource->res_type = res_type;
			ret.resource->ready_bit = false;

			return ret;
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
		static resource_manager* m_instance;

		resource_manager(const base_info& base);

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
		//std::atomic_bool m_fences_sync_bit;

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
		array<dp_pool, DSL_TYPE_COUNT> m_dp_pools;

		//utility objects
		VkDescriptorSetLayout m_dsls[DSL_TYPE_COUNT];
		VkBuffer m_staging_buffer;
		VkFence m_build_f;

		VkCommandPool m_build_cp;
		VkCommandBuffer m_build_cb;

		VkSampler m_samplers[SAMPLER_TYPE_COUNT];


		template<size_t res_type>
		void build(base_resource* res, const char* build_info);

		template<uint32_t res_type>
		void destroy(base_resource* res);

		void build_loop();
		void destroy_loop();

		void begin_build_cb();
		void end_build_cb(const VkSemaphore* wait_semaphores=nullptr, const VkPipelineStageFlags* wait_flags=nullptr, 
			uint32_t wait_count=0);
		void wait_for_build_fence();
		void create_dsls();
		void create_dp_pools();
		void create_command_pool();
		void create_samplers();
		void create_memory_resources_and_containers();

	};
}
