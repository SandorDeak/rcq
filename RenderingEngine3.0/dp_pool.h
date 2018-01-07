#pragma once

#include "vulkan.h"
#include "host_memory.h"
#include "vector.h"

#include <assert.h>

namespace rcq
{
	class dp_pool
	{
		struct dp
		{
			VkDescriptorPool pool;
			uint32_t free_count;
		};

	public:
		dp_pool() : m_create_info{}  {}

		dp_pool(uint32_t pool_sizes_count, uint32_t dp_capacity, uint32_t max_set_count, VkDevice device, host_memory* memory) :
			m_pool_sizes(memory, pool_sizes_count),
			m_dps(memory),
			m_create_info{},
			m_device(device),
			m_dp_capacity(dp_capacity)
		{
			m_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			m_create_info.maxSets = max_set_count;
			m_create_info.pPoolSizes = m_pool_sizes.data();
			m_create_info.poolSizeCount = pool_sizes_count;
		}

		~dp_pool()
		{
			for (auto& dp : m_dps)
				vkDestroyDescriptorPool(m_device, dp.pool, m_vk_alloc);
		}

		void init(uint32_t pool_sizes_count, uint32_t dp_capacity, uint32_t max_set_count, VkDevice device, host_memory* memory)
		{
			m_pool_sizes.init(memory, pool_sizes_count);
			m_dps.init(memory);
			m_device = device;
			m_dp_capacity = dp_capacity;

			m_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			m_create_info.maxSets = max_set_count;
			m_create_info.pPoolSizes = m_pool_sizes.data();
			m_create_info.poolSizeCount = pool_sizes_count;
		}

		void reset()
		{
			for (auto& dp : m_dps)
				vkDestroyDescriptorPool(m_device, dp.pool, m_vk_alloc);

			m_dps.reset();
			m_pool_sizes.reset();
		}


		VkDescriptorPoolSize* sizes()
		{
			return m_pool_sizes.data();
		}

		VkDescriptorPool use_dp(uint32_t& dp_id)
		{
			dp_id = 0;
			for (auto it = m_dps.begin(); it != m_dps.end(); ++it)
			{
				if (it->free_count > 0)
				{
					--it->free_count;
					return it->pool;
				}
				++dp_id;
			}

			auto new_dp = m_dps.push_back();
			new_dp->free_count = m_dp_capacity - 1;

			assert(vkCreateDescriptorPool(m_device, &m_create_info, m_vk_alloc, &new_dp->pool) == VK_SUCCESS);

			return new_dp->pool;
		}

		VkDescriptorPool stop_using_dp(uint32_t dp_id)
		{
			++m_dps[dp_id].free_count;
			return m_dps[dp_id].pool;
		}

	private:
		VkDevice m_device;
		const VkAllocationCallbacks* m_vk_alloc;
		vector<dp> m_dps;
		vector<VkDescriptorPoolSize> m_pool_sizes;
		VkDescriptorPoolCreateInfo m_create_info;
		uint32_t m_dp_capacity;
	};
}