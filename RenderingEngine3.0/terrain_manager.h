#pragma once

#include "foundation2.h"
#include "vk_memory_resource.h"
#include "freelist_resource_host.h"
#include "queue.h"
#include "vector.h"

namespace rcq
{
	class terrain_manager
	{
	public:

		void poll_requests();
		void poll_results();
		void init(const glm::uvec2& tile_count, const glm::uvec2& tile_size, uint32_t mip_level_count, VkCommandPool cp);
		void destroy();

		void set_terrain(resource<RES_TYPE_TERRAIN>* t)
		{
			m_terrain = t;
		}

		static terrain_manager* instance()
		{
			return m_instance;
		}

	private:
		terrain_manager(const base_info& base, memory_resource* host_memory_resource, device_memory_resource* device_memory_res);
		~terrain_manager();

		static terrain_manager* m_instance;
		void loop();
		void decrease_min_mip_level(const glm::uvec2& tile_id);
		void increase_min_mip_level(const glm::uvec2& tile_id);

		struct request_data
		{
			static const uint32_t max_request_count = 256;
			uint32_t requests[max_request_count];
			uint32_t request_count;
			uint32_t padding;
		};

		const base_info& m_base;

		resource<RES_TYPE_TERRAIN>* m_terrain;

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
		vk_memory_resource m_mappable_memory_resource;
		pool_memory_resource m_page_pool;
		freelist_resource_host m_host_memory_res;
		vk_allocator m_vk_alloc;

		queue<uint32_t> m_request_queue;
		//std::mutex m_request_queue_mutex;

		queue<uint32_t> m_result_queue;
		//std::mutex m_result_queue_mutex;
		std::thread m_thread;
		bool m_should_end;

		VkCommandBuffer m_cb;
		VkCommandPool m_cp;
		VkSemaphore m_binding_finished_s;
		VkFence m_copy_finished_f;
		vector<vector<vector<vector<uint64_t>>>> m_pages; //mip_level, tile_id.x // tile_id.y

		glm::uvec2 m_tile_count;
		glm::uvec2 m_tile_size;
		glm::uvec2 m_tile_size_in_pages;
		uint32_t m_page_size;
	};
}

