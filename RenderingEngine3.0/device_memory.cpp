#include "device_memory.h"

using namespace rcq;

//const size_t device_memory::BLOCK_SIZE[USAGE_COUNT] = { 128, 128 };

device_memory* device_memory::m_instance = nullptr;

device_memory::device_memory(const base_info& base) : m_base(base), m_alloc_info{}, m_alloc("device_memory")
{
	m_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	vkGetPhysicalDeviceMemoryProperties(m_base.physical_device, &m_memory_properties);

	/*VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(m_base.physical_device, &props);
	std::cout << props.deviceName << std::endl;
	size_t alignment = static_cast<size_t>(props.limits.minUniformBufferOffsetAlignment);

	constexpr size_t ideal_cell_size = std::max(sizeof(transform_data), sizeof(material_opaque_data));

	// = calc_offset(alignment, ideal_cell_size);

	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_info.size = ideal_cell_size;
	buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VkBuffer temp_buffer;
	if (vkCreateBuffer(m_base.device, &buffer_info, m_alloc, &temp_buffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create buffer!");

	VkMemoryRequirements mr;
	vkGetBufferMemoryRequirements(m_base.device, temp_buffer, &mr);
	m_cell_size[USAGE_STATIC] = calc_offset(alignment, static_cast<size_t>(mr.size));

	m_alloc_info[USAGE_STATIC].sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	m_alloc_info[USAGE_STATIC].allocationSize = m_cell_size[USAGE_STATIC]*BLOCK_SIZE[USAGE_STATIC];
	m_alloc_info[USAGE_STATIC].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vkDestroyBuffer(m_base.device, temp_buffer, m_alloc);

	buffer_info.size = ideal_cell_size;
	buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (vkCreateBuffer(m_base.device, &buffer_info, m_alloc, &temp_buffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create buffer!");

	vkGetBufferMemoryRequirements(m_base.device, temp_buffer, &mr);
	m_cell_size[USAGE_DYNAMIC] = calc_offset(alignment, static_cast<size_t>(mr.size));

	m_alloc_info[USAGE_DYNAMIC].sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	m_alloc_info[USAGE_DYNAMIC].allocationSize = m_cell_size[USAGE_DYNAMIC]*BLOCK_SIZE[USAGE_DYNAMIC];
	m_alloc_info[USAGE_DYNAMIC].memoryTypeIndex = find_memory_type(m_base.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkDestroyBuffer(m_base.device, temp_buffer, m_alloc);*/
}


device_memory::~device_memory()
{
	for (auto& blocks : m_blocks)
	{
		for (auto block : blocks)
		{
			if (block != VK_NULL_HANDLE)
				vkFreeMemory(m_base.device, block, m_alloc);
		}
	}
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

cell_info device_memory::alloc_buffer_memory(USAGE usage, VkBuffer buffer, void** mapped_data)
{
	std::lock_guard<std::mutex> lock(m_block_mutexes[usage]);

	if (!m_available_cells[usage].empty())
	{
		auto it = m_available_cells[usage].begin();
		cell_info cell = *it;
		m_available_cells[usage].erase(it);
		vkBindBufferMemory(m_base.device, buffer, m_blocks[usage][cell.first], cell.second);
		if (usage == USAGE_DYNAMIC)
		{
			vkMapMemory(m_base.device, m_blocks[USAGE_DYNAMIC][cell.first], cell.second, m_cell_size[USAGE_DYNAMIC], 0, mapped_data);
		}
		return cell;
	}

	VkDeviceMemory new_block;
	if (vkAllocateMemory(m_base.device, m_alloc_info + usage, m_alloc, &new_block) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate memory!");
	uint32_t block_id = m_blocks[usage].size();
	m_blocks[usage].push_back(new_block);
	auto offset = m_cell_size[usage];
	for (size_t i = 0; i < BLOCK_SIZE[usage] - 1; ++i)
	{
		m_available_cells[usage].emplace_hint(m_available_cells[usage].end(), block_id, offset);
		offset += m_cell_size[usage];
	}
	vkBindBufferMemory(m_base.device, buffer, m_blocks[usage][block_id], 0);
	if (usage == USAGE_DYNAMIC)
	{
		vkMapMemory(m_base.device, m_blocks[USAGE_DYNAMIC][block_id], 0, m_cell_size[USAGE_DYNAMIC], 0, mapped_data);
	}
	return { block_id, 0 };
}

void device_memory::free_buffer(USAGE usage, const cell_info& cell)
{
	std::lock_guard<std::mutex> lock(m_block_mutexes[usage]);
	m_available_cells[usage].insert(cell);
	
	if (auto range = m_available_cells[usage].equal_range(cell.first);
		std::distance(range.first, range.second) ==BLOCK_SIZE[usage] && m_available_cells[usage].size() > BLOCK_SIZE[usage])
	{
		m_available_cells[usage].erase(range.first, range.second);
		vkFreeMemory(m_base.device, m_blocks[usage][cell.first], m_alloc); 
		m_blocks[usage][cell.first] = VK_NULL_HANDLE;
	}
}