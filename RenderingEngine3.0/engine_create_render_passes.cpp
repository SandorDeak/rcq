#include "engine.h"

#include "rps.h"

using namespace rcq;

template<uint32_t rp_type>
void engine::create_render_pass_impl(VkRenderPass* rp)
{
	assert(vkCreateRenderPass(m_base.device, &rp_create_info<rp_type>::create_info, m_vk_alloc, rp) == VK_SUCCESS);
}

template<uint32_t... rp_types>
void engine::create_render_passes_impl(std::index_sequence<rp_types...>)
{
	auto l = { (create_render_pass_impl<rp_types>(m_rps + rp_types), 0), ... };
}

void engine::create_render_passes()
{
	create_render_passes_impl(std::make_index_sequence<RP_COUNT>());
}