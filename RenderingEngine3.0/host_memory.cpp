#include "host_memory.h"

using namespace rcq;

host_memory* rcq::host_memory::m_instance = nullptr;

const size_t MEMORY_SIZE = 1024 * 1024 * 1024;
const size_t MAX_ALIGNMENT = 256;

host_memory::host_memory() :
	m_os_resource()
{
	//m_base_resource = new freelist_resource_host(MEMORY_SIZE, MAX_ALIGNMENT, &m_os_resource);
	m_base_resource = new freelist_resource<size_t>(MEMORY_SIZE, MAX_ALIGNMENT, &m_os_resource, &m_os_resource);
}


host_memory::~host_memory()
{
	delete m_base_resource;
}

void host_memory::init()
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("host memory is already initialised!");
	}

	m_instance = new host_memory();
}

void host_memory::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy host memory, it doesn't exist!");
	}

	delete m_instance;
}
