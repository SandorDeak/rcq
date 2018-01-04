#include "resource_manager.h"

using namespace rcq;

template<>
void resource_manager::destroy<RES_TYPE_MESH>(base_resource* res)
{
	auto m = reinterpret_cast<resource<RES_TYPE_MESH>*>(res->data);

	vkDestroyBuffer(m_base.device, m->vb, m_vk_alloc);
	m_device_memory.deallocate(m->vb_offset);

	vkDestroyBuffer(m_base.device, m->ib, m_vk_alloc);
	m_device_memory.deallocate(m->ib_offset);

	if (m->veb_offset != 0)
	{
		vkDestroyBuffer(m_base.device, m->veb, m_vk_alloc);
		m_device_memory.deallocate(m->veb);
	}

	m_resource_pool.deallocate(reinterpret_cast<size_t>(res));
}

template<>
void resource_manager::destroy<RES_TYPE_MAT_OPAQUE>(base_resource* res)
{
	auto mat = reinterpret_cast<resource<RES_TYPE_MAT_OPAQUE>*>(res->data);

	vkFreeDescriptorSets(m_base.device, m_dp_pools[DSL_TYPE_MAT_OPAQUE].stop_using_dp(mat->dp_index), 1, &mat->ds);


	for (size_t i = 0; i < TEX_TYPE_COUNT; ++i)
	{
		if (mat->texs[i].offset != 0)
		{
			vkDestroyImageView(m_base.device, mat->texs[i].view, m_vk_alloc);
			vkDestroyImage(m_base.device, mat->texs[i].image, m_vk_alloc);
			vkDestroySampler(m_base.device, mat->texs[i].sampler, m_vk_alloc);
			m_device_memory.deallocate(mat->texs[i].offset);
		}
	}

	vkDestroyBuffer(m_base.device, mat->data_buffer, m_vk_alloc);
	m_device_memory.deallocate(mat->data_offset);

	m_resource_pool.deallocate(reinterpret_cast<size_t>(res));
}

template<>
void resource_manager::destroy<RES_TYPE_TR>(base_resource* res)
{
	auto tr = reinterpret_cast<resource<RES_TYPE_TR>*>(res->data);

	vkFreeDescriptorSets(m_base.device, m_dp_pools[DSL_TYPE_TR].stop_using_dp(tr->dp_index), 1, &tr->ds);
	vkDestroyBuffer(m_base.device, tr->data_buffer, m_vk_alloc);
	m_device_memory.deallocate(tr->data_offset);

	m_resource_pool.deallocate(reinterpret_cast<size_t>(res));
}

template<>
void resource_manager::destroy<RES_TYPE_SKY>(base_resource* res)
{
	auto s = reinterpret_cast<resource<RES_TYPE_SKY>*>(res->data);

	vkFreeDescriptorSets(m_base.device, m_dp_pools[DSL_TYPE_SKY].stop_using_dp(s->dp_index), 1, &s->ds);

	for (uint32_t i = 0; i<3; ++i)
	{
		vkDestroyImageView(m_base.device, s->tex[i].view, m_vk_alloc);
		vkDestroyImage(m_base.device, s->tex[i].image, m_vk_alloc);
		m_device_memory.deallocate(s->tex[i].offset);
	}
	vkDestroySampler(m_base.device, s->sampler, m_vk_alloc);

	m_resource_pool.deallocate(reinterpret_cast<size_t>(res));
}




template<>
void resource_manager::destroy<RES_TYPE_TERRAIN>(base_resource* res)
{
	auto t = reinterpret_cast<resource<RES_TYPE_TERRAIN>*>(res->data);

	VkDescriptorSet dss[2] = { t->ds, t->request_ds };
	vkFreeDescriptorSets(m_base.device, m_dp_pools[DSL_TYPE_TERRAIN].stop_using_dp(t->dp_index), 2, dss);

	vkDestroyImageView(m_base.device, t->tex.view, m_vk_alloc);
	vkDestroyImage(m_base.device, t->tex.image, m_vk_alloc);
	vkDestroySampler(m_base.device, t->tex.sampler, m_vk_alloc);
	m_device_memory.deallocate(t->tex.dummy_page_offset);
	m_device_memory.deallocate(t->tex.mip_tail_offset);

	vkDestroyBuffer(m_base.device, t->data_buffer, m_vk_alloc);
	m_device_memory.deallocate(t->data_offset);

	vkDestroyBuffer(m_base.device, t->request_data_buffer, m_vk_alloc);
	m_device_memory.deallocate(t->request_data_offset);

	/*vkDestroyBufferView(m_base.device, t.current_mip_levels_view, m_alloc);
	vkDestroyBuffer(m_base.device, t.current_mip_levels_buffer, m_alloc);
	vkFreeMemory(m_base.device, t.current_mip_levels_memory, m_alloc);

	vkDestroyBufferView(m_base.device, t.requested_mip_levels_view, m_alloc);
	vkDestroyBuffer(m_base.device, t.requested_mip_levels_buffer, m_alloc);
	vkFreeMemory(m_base.device, t.requested_mip_levels_memory, m_alloc);*/

	m_resource_pool.deallocate(reinterpret_cast<size_t>(res));
}

template<>
void resource_manager::destroy<RES_TYPE_WATER>(base_resource* res)
{
	auto w = reinterpret_cast<resource<RES_TYPE_WATER>*>(res->data);

	VkDescriptorSet dss[2] = { w->ds, w->fft_ds };
	vkFreeDescriptorSets(m_base.device, m_dp_pools[DSL_TYPE_WATER].stop_using_dp(w->dp_index), 2, dss);

	vkDestroyBuffer(m_base.device, w->fft_params_buffer, m_vk_alloc);
	m_device_memory.deallocate(w->fft_params_offset);

	vkDestroyImageView(m_base.device, w->noise.view, m_vk_alloc);
	vkDestroyImageView(m_base.device, w->tex.view, m_vk_alloc);
	vkDestroyImage(m_base.device, w->noise.image, m_vk_alloc);
	vkDestroyImage(m_base.device, w->tex.image, m_vk_alloc);
	vkDestroySampler(m_base.device, w->sampler, m_vk_alloc);
	m_device_memory.deallocate(w->noise.offset);
	m_device_memory.deallocate(w->tex.offset);

	m_resource_pool.deallocate(reinterpret_cast<size_t>(res));
}