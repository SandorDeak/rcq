#include "device_memory.h"

using namespace rcq;

const size_t device_memory::BLOCK_SIZE[USAGE_COUNT] = { 128, 128 };

device_memory* device_memory::m_instance = nullptr;

device_memory::device_memory(const base_info& base) : m_base(base)
{

}


device_memory::~device_memory()
{
}

void device_memory::init(const base_info& base)
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("core is already initialised!");
	}
	m_instance = new device_memory(base);
}

void device_memory::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy core, it doesn't exist!");
	}

	delete m_instance;
}

cell_info device_memory::alloc_buffer(USAGE usage, VkBuffer buffer, char** data)
{
	std::lock_guard<std::mutex> lock(m_block_mutexes[usage]);

	if (!m_available_cells[usage].empty())
	{
		auto it = m_available_cells[usage].begin();
		cell_info cell = *it;
		m_available_cells[usage].erase(it);
		vkBindBufferMemory(m_base.device, buffer, m_blocks[usage][cell.first], cell.second);
		if (usage==USAGE_DYNAMIC)
			vkMapMemory(m_base.device, m_blocks[usage][cell.first], cell.second, m_cell_size, 0, reinterpret_cast<void**>(data));
		return cell;
	}

	VkDeviceMemory new_block;
	if (vkAllocateMemory(m_base.device, m_alloc_info + usage, host_memory_manager, &new_block) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate memory!");
	uint32_t block_id = m_blocks[usage].size();
	m_blocks[usage].push_back(new_block);
	auto offset = m_cell_size;
	for (size_t i = 0; i < BLOCK_SIZE[usage] - 1; ++i)
	{
		m_available_cells[usage].emplace_hint(m_available_cells[usage].end(), block_id, offset);
		offset += m_cell_size;
	}
	vkBindBufferMemory(m_base.device, buffer, m_blocks[usage][block_id], 0);
	return { block_id, 0 };
}

void device_memory::free_buffer(USAGE usage, const cell_info& cell)
{
	std::lock_guard<std::mutex> lock(m_block_mutexes[usage]);
	m_available_cells[usage].insert(cell);

	if (auto range = m_available_cells[usage].equal_range(cell.first);
		std::distance(range.second, range.first)==BLOCK_SIZE[usage] && m_available_cells[usage].size() > BLOCK_SIZE[usage])
	{
		m_available_cells[usage].erase(range.first, range.second);
		vkFreeMemory(m_base.device, m_blocks[usage][cell.first], host_memory_manager);
	}
}