#include "device_memory.h"

using namespace rcq;

//const size_t device_memory::BLOCK_SIZE[USAGE_COUNT] = { 128, 128 };

const uint64_t DEVICE_LOCAL_MEMORY_SIZE = 2 * 1024 * 1024 * 1024;

device_memory* device_memory::m_instance = nullptr;

device_memory::device_memory(const base_info& base, memory_resource* host_memory_resource) : 
	m_base(base),
	m_vk_alloc(host_memory_resource)
	//m_memory_res_available(true)
{

	m_vk_memory_res = new(reinterpret_cast<void*>(m_host_memory_resource->allocate(sizeof(vk_memory_resource), alignof(vk_memory_resource))))
		vk_memory_resource(m_base.device, find_memory_type(~0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), &m_vk_alloc);

	m_memory_res = new(reinterpret_cast<void*>(m_host_memory_resource->allocate(sizeof(freelist_resource), alignof(freelist_resource))))
		freelist_resource(DEVICE_LOCAL_MEMORY_SIZE, m_vk_memory_res->max_alignment(), m_vk_memory_res, m_host_memory_resource);
}


device_memory::~device_memory()
{
	m_memory_res->~freelist_resource();
	m_vk_memory_res->~vk_memory_resource();
	m_host_memory_resource->deallocate(reinterpret_cast<uint64_t>(m_memory_res));
	m_host_memory_resource->deallocate(reinterpret_cast<uint64_t>(m_vk_memory_res));
}

void device_memory::init(const base_info& base, memory_resource* host_memory_resource)
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("core is already initialised!");
	}
	m_host_memory_resource = host_memory_resource;

	m_instance = new(reinterpret_cast<void*>(m_host_memory_resource->allocate(sizeof(device_memory), alignof(device_memory))))
		device_memory(base, host_memory_resource);
}

void device_memory::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy core, it doesn't exist!");
	}

	m_instance->~device_memory();
	m_host_memory_resource->deallocate(reinterpret_cast<uint64_t>(m_instance));
	m_instance = nullptr;
}

uint32_t device_memory::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(m_base.physical_device, &memory_properties);

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
	{
		if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties))
			return i;
	}

	throw std::runtime_error("failed to find suitable memory type!");
}