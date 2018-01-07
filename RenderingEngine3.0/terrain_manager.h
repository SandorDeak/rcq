#pragma once

#include "vk_memory.h"
#include "vk_allocator.h"
#include "freelist_host_memory.h"
#include "pool_device_memory.h"

#include "queue.h"
#include "vector.h"

#include "resources.h"
#include "base_info.h"

#include <thread>

namespace rcq
{
	class terrain_manager
	{
	public:

		void poll_requests();
		void poll_results();
		static void init(const base_info& base);
		static void destroy();

		void init_resources(const glm::uvec2& tile_count, const glm::uvec2& tile_size, uint32_t mip_level_count);
		void destroy_resources();

		void set_terrain(resource<RES_TYPE_TERRAIN>* t)
		{
			m_terrain = t;
		}

		static terrain_manager* instance()
		{
			return m_instance;
		}

	private:
		//ctor, dtor, singleton pattern
		terrain_manager(const base_info& base);
		~terrain_manager();
		terrain_manager(const terrain_manager&) = delete;
		terrain_manager(terrain_manager&&) = delete;
		terrain_manager& operator=(const terrain_manager&) = delete;
		terrain_manager& operator=(terrain_manager&&) = delete;
		static terrain_manager* m_instance;

		//create functions
		void create_memory_resources_and_containers();
		void create_cp_allocate_cb();

		//request managing
		void loop();
		void decrease_min_mip_level(const glm::uvec2& tile_id);
		void increase_min_mip_level(const glm::uvec2& tile_id);

		struct request_data
		{
			static const uint32_t max_request_count = 256;
			uint32_t requests[max_request_count];
			uint32_t request_count;
			uint32_t padding[3];
		};

		const base_info& m_base;

		//terrain resource
		resource<RES_TYPE_TERRAIN>* m_terrain;

		//buffers, offsets, pointers
		VkBuffer m_mapped_buffer;
		uint64_t m_mapped_offset;
		request_data* m_request_data;
		VkBufferView m_requested_mip_levels_view;
		float* m_requested_mip_levels;
		VkBufferView m_current_mip_levels_view;
		float* m_current_mip_levels;
		uint64_t m_pages_staging_offset;
		char* m_pages_staging;

		//memory resources
		freelist_host_memory m_host_memory;
		vk_allocator m_vk_alloc;

		vk_memory m_mappable_memory;

		vk_memory m_vk_page_pool;
		pool_device_memory m_page_pool;
		
		//queues
		queue<uint32_t> m_request_queue;
		queue<uint32_t> m_result_queue;

		//thread
		std::thread m_thread;
		bool m_should_end;

		//synchronization objects
		VkSemaphore m_binding_finished_s;
		VkFence m_copy_finished_f;

		//cp, cb
		VkCommandBuffer m_cb;
		VkCommandPool m_cp;
		
		//data
		vector<vector<vector<vector<uint64_t>>>> m_pages; //mip_level, tile_id.x // tile_id.y
		glm::uvec2 m_tile_count;
		glm::uvec2 m_tile_size;
		glm::uvec2 m_tile_size_in_pages;
		uint32_t m_page_size;
	};
}