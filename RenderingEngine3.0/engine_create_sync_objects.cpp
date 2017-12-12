#include "engine.h"

using namespace rcq;

void engine::create_sync_objects()
{
	//semaphores
	VkSemaphoreCreateInfo s = {};
	s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (auto& sem : m_semaphores)
		assert(vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &sem) == VK_SUCCESS);
	for (auto& sem : m_present_ready_ss)
		assert(vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &sem) == VK_SUCCESS);

	//fences
	VkFenceCreateInfo f = {};
	f.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	f.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (auto& fen : m_fences)
		assert(vkCreateFence(m_base.device, &f, m_vk_alloc, &fen) == VK_SUCCESS);

	//events
	VkEventCreateInfo e = {};
	e.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	for(auto& ev : m_events)
		assert(vkCreateEvent(m_base.device, &e, m_vk_alloc, &ev) == VK_SUCCESS);
}