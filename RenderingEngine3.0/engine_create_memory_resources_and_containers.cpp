#include "engine.h"

#include "os_memory.h"

#include "utility.h"

#include "const_max_alignment.h"

using namespace rcq;

void engine::create_memory_resources_and_containers()
{
	m_host_memory.init(256 * 1024 * 1024, MAX_ALIGNMENT, &OS_MEMORY);

	m_vk_alloc.init(&m_host_memory);

	m_vk_device_memory.init(m_base.device, utility::find_memory_type(m_base.physical_device, ~0,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), &m_vk_alloc);
	m_device_memory.init(512 * 1024 * 1024, MAX_ALIGNMENT, &m_vk_device_memory, &m_host_memory);

	m_vk_mappable_memory.init(m_base.device, utility::find_memory_type(m_base.physical_device, ~0,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), &m_vk_alloc);

	m_mappable_memory.init(1024 * 1024, MAX_ALIGNMENT, &m_vk_mappable_memory, &m_host_memory);

	m_opaque_objects.init(64, &m_host_memory);
}