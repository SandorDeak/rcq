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

	m_vk_mappable_memory.init(m_base.device, utility::find_memory_type(m_base.physical_device, ~0,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), &m_vk_alloc);

	m_mappable_memory.init(256 * 1024 * 1024, 256, &m_vk_mappable_memory, &m_host_memory);

	m_vk_device_memory.init(m_base.device, utility::find_memory_type(m_base.physical_device, ~0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
		&m_vk_alloc);
	constexpr VkDeviceSize SIZE = VkDeviceSize(2)*VkDeviceSize(1024)*VkDeviceSize(1024)*VkDeviceSize(1024);
	m_device_memory.init(SIZE, 256, &m_vk_device_memory, &m_host_memory);

	m_build_queue.init(&m_host_memory);
	m_build_queue.init_buffer();

	m_destroy_queue.init(&m_host_memory);
	m_destroy_queue.init_buffer();
}