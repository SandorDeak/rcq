#include "gta5_pass.h"

#include "dp.h"

using namespace rcq;

void gta5_pass::create_descriptor_pool()
{
	assert(vkCreateDescriptorPool(m_base.device, &dp::create_info, m_vk_alloc, &m_dp) == VK_SUCCESS);
}