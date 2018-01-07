#include "resource_manager.h"

#include "os_memory.h"

#include "utility.h"

#include "const_max_alignment.h"

using namespace rcq;

void resource_manager::create_memory_resources_and_containers()
{
	const uint64_t HOST_MEMORY_SIZE = 256 * 1024 * 1024;
	m_host_memory.init(HOST_MEMORY_SIZE, MAX_ALIGNMENT, &OS_MEMORY);

	m_resource_pool.init(sizeof(base_resource), alignof(base_resource), &m_host_memory);
	m_vk_alloc.init(&m_host_memory);

	m_vk_mappable_memory.init(m_base.device, MEMORY_TYPE_HVC, &m_vk_alloc);

	m_vk_dl0_memory.init(m_base.device, MEMORY_TYPE_DL0, &m_vk_alloc);
	constexpr VkDeviceSize SIZE = VkDeviceSize(1)*VkDeviceSize(1024)*VkDeviceSize(1024)*VkDeviceSize(1024);
	m_dl0_memory.init(SIZE, m_vk_dl0_memory.max_alignment(), &m_vk_dl0_memory, &m_host_memory);

	m_vk_dl1_memory.init(m_base.device, MEMORY_TYPE_DL1, &m_vk_alloc);
	m_dl1_memory.init(SIZE, m_vk_dl1_memory.max_alignment(), &m_vk_dl1_memory, &m_host_memory);

	m_build_queue.init(&m_host_memory);
	m_build_queue.init_buffer();

	m_destroy_queue.init(&m_host_memory);
	m_destroy_queue.init_buffer();
}