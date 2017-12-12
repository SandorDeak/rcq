#include "resource_manager.h"

using namespace rcq;

void resource_manager::create_dp_pools()
{
	const uint32_t TR_POOL_CAPACITY = 64;
	const uint32_t MAT_OPAQUE_CAPACITY = 64;

	//create tr dp_pool
	{
		auto& pool = m_dp_pools[DSL_TYPE_TR];

		pool.init(1, TR_POOL_CAPACITY, m_base.device, &m_host_memory);
		pool.sizes()[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.sizes()[0].descriptorCount = TR_POOL_CAPACITY;
	}

	//create sky pool
	{
		auto& pool = m_dp_pools[DSL_TYPE_SKY];
		pool.init(1, 1, m_base.device, &m_host_memory);
		pool.sizes()[0].descriptorCount = 3;
		pool.sizes()[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	}

	//create water pool
	{
		auto& pool = m_dp_pools[DSL_TYPE_WATER];
		pool.init(2, 1, m_base.device, &m_host_memory);
		pool.sizes()[0].descriptorCount = 1;
		pool.sizes()[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.sizes()[1].descriptorCount = 1;
		pool.sizes()[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool.sizes()[2].descriptorCount = 2;
		pool.sizes()[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	}

	//create terrain pool
	{
		auto& pool = m_dp_pools[DSL_TYPE_TERRAIN];
		pool.init(5, 1, m_base.device, &m_host_memory);
		pool.sizes()[0].descriptorCount = 1;
		pool.sizes()[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool.sizes()[1].descriptorCount = 1;
		pool.sizes()[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		pool.sizes()[2].descriptorCount = 1;
		pool.sizes()[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.sizes()[3].descriptorCount = 1;
		pool.sizes()[3].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		pool.sizes()[4].descriptorCount = 1;
		pool.sizes()[4].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}

	//create mat opaque pool
	{
		auto& pool = m_dp_pools[DSL_TYPE_MAT_OPAQUE];
		pool.init(2, MAT_OPAQUE_CAPACITY, m_base.device, &m_host_memory);
		pool.sizes()[0].descriptorCount = MAT_OPAQUE_CAPACITY;
		pool.sizes()[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.sizes()[1].descriptorCount = TEX_TYPE_COUNT*MAT_OPAQUE_CAPACITY;
		pool.sizes()[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	}
}