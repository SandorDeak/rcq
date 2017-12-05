#pragma once


#include "foundation2.h"
#include "queue.h"
#include "stack.h"
#include "array.h"
#include "pool_memory_resource_host.h"
#include "monotonic_buffer_resource.h"
#include "vk_allocator.h"
#include "vk_memory_resource.h"
#include "freelist_resource.h"
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

		static void init(const base_info& info, memory_resource* memory_res);
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

	private:
		static resource_manager* m_instance;

		resource_manager(const base_info& base);

		const base_info& m_base;

		//memory resources
		static memory_resource* m_memory_resource;
		pool_memory_resource_host m_resource_pool;
		vk_allocator m_vk_alloc;

		vk_memory_resource m_vk_mappable_memory;
		monotonic_buffer_resource m_mappable_memory;
		
		freelist_resource m_device_memory_res;


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
		//stack<VkFence> m_fences;
		array<dp_pool, DSL_TYPE_COUNT> m_dp_pools;

		//utility objects
		array<VkDescriptorSetLayout, DSL_TYPE_COUNT> m_dsls;
		VkBuffer m_staging_buffer;
		VkFence m_build_f;

		VkCommandPool m_build_cp;
		VkCommandBuffer m_build_cb;

		VkSampler m_samplers[SAMPLER_TYPE_COUNT];


		//void load_texture(const char* filename, uint64_t& staging_buffer_offset);


		template<size_t res_type>
		void build(base_resource* res, const char* build_info);

		template<uint32_t res_type>
		void destroy(base_resource* res);

		void build_loop()
		{
			while (!m_should_end_build)
			{
				if (m_build_queue.empty())
				{
					std::unique_lock<std::mutex> lock;
					while (m_build_queue.empty())
						m_build_queue_cv.wait(lock);
				}

				base_resource_build_info* info = m_build_queue.front(); 
				switch (info->resource_type)
				{
				case 0:
					build<0>(info->base_res, info->data);
					break;
				case 1:
					build<1>(info->base_res, info->data);
					break;
				case 2:
					build<2>(info->base_res, info->data);
					break;
				case 3:
					build<3>(info->base_res, info->data);
					break;
				case 4:
					build<4>(info->base_res, info->data);
				case 5:
					build<5>(info->base_res, info->data);
					break;
				case 6:
					build<6>(info->base_res, info->data);
					break;
				case 7:
					build<7>(info->base_res, info->data);
					break;
				}
				static_assert(8 == RES_TYPE_COUNT);

				m_build_queue.pop();

			}
		}

		void destroy_loop()
		{
			while (!m_should_end_destroy)
			{
				if (m_destroy_queue.empty())
				{
					std::unique_lock<std::mutex> lock(m_destroy_queue_mutex);
					while (m_destroy_queue.empty())
						m_destroy_queue_cv.wait(lock);
				}

				base_resource* base_res = *m_destroy_queue.front();
				
				while (!base_res->ready_bit.load());

				switch (base_res->res_type)
				{
				case 0:
					destroy<0>(base_res);
					break;
				case 1:
					destroy<1>(base_res);
					break;
				case 2:
					destroy<2>(base_res);
					break;
				case 3:
					destroy<3>(base_res);
					break;
				case 4:
					destroy<4>(base_res);
					break;
				case 5:
					destroy<5>(base_res);
					break;
				case 6:
					destroy<6>(base_res);
					break;
				case 7:
					destroy<7>(base_res);
					break;
				}
				static_assert(8 == RES_TYPE_COUNT);

				m_destroy_queue.pop();
			}
		}

		void begin_build_cb();
		void end_build_cb();
		void wait_for_build_fence();
		void create_dsls();
		void create_dp_pools();
		void create_command_pool();
		void create_samplers();

	};
}
