#include "engine.h"

#include "os_memory.h"

#include "utility.h"

#include "const_max_alignment.h"

using namespace rcq;

void engine::create_memory_resources_and_containers()
{
	m_host_memory.init(256 * 1024 * 1024, MAX_ALIGNMENT, &OS_MEMORY);

	m_vk_alloc.init(&m_host_memory);

	m_vk_dl0_memory.init(m_base.device, MEMORY_TYPE_DL0, &m_vk_alloc);
	m_vk_dl1_memory.init(m_base.device, MEMORY_TYPE_DL1, &m_vk_alloc);
	m_dl0_memory.init(512 * 1024 * 1024, /*MAX_ALIGNMENT*/1024, &m_vk_dl0_memory, &m_host_memory);
	m_dl1_memory.init(512 * 1024 * 1024, /*MAX_ALIGNMENT*/1024, &m_vk_dl1_memory, &m_host_memory);

	m_vk_mappable_memory.init(m_base.device, MEMORY_TYPE_HVC, &m_vk_alloc);

	m_mappable_memory.init(1024 * 1024, /*MAX_ALIGNMENT*/1024, &m_vk_mappable_memory, &m_host_memory);

	m_opaque_objects.init(64, &m_host_memory);
	m_opaque_objects_changed = true;
}