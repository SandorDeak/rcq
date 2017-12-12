#include "engine.h"

using namespace rcq;

engine* engine::m_instance = nullptr;

engine::engine(const base_info& info) : m_base(info)
{
	create_memory_resources_and_containers();
	create_buffers_and_images();
	create_render_passes();
	create_graphics_pipelines();
	create_compute_pipelines();
	create_samplers();
	create_descriptor_pool();
	allocate_and_update_dss();
	create_framebuffers();
	allocate_and_record_cbs();
	create_sync_objects();
}


engine::~engine()
{
	assert(m_opaque_objects.size() == 0);

	vkQueueWaitIdle(m_base.graphics_queue);
	vkQueueWaitIdle(m_base.compute_queue);
	vkQueueWaitIdle(m_base.present_queue);

	for (auto& s : m_semaphores)
		vkDestroySemaphore(m_base.device, s, m_vk_alloc);
	for (auto& s : m_present_ready_ss)
		vkDestroySemaphore(m_base.device, s, m_vk_alloc);
	for (auto& f : m_fences)
		vkDestroyFence(m_base.device, f, m_vk_alloc);
	for (auto& e : m_events)
		vkDestroyEvent(m_base.device, e, m_vk_alloc);
	for (auto& fb : m_fbs)
		vkDestroyFramebuffer(m_base.device, fb, m_vk_alloc);
	for(auto& fb : m_postprocessing_fbs)
		vkDestroyFramebuffer(m_base.device, fb, m_vk_alloc);
	for (auto& cp : m_cpools)
		vkDestroyCommandPool(m_base.device, cp, m_vk_alloc);
	for (auto& gp : m_gps)
		vkDestroyPipeline(m_base.device, gp, m_vk_alloc);
	for (auto& cp : m_cps)
		vkDestroyPipeline(m_base.device, cp, m_vk_alloc);
	for (auto& rp : m_rps)
		vkDestroyRenderPass(m_base.device, rp, m_vk_alloc);
	for (auto& s : m_samplers)
		vkDestroySampler(m_base.device, s, m_vk_alloc);
	for (auto& im : m_res_image)
	{
		vkDestroyImageView(m_base.device, im.view, m_vk_alloc);
		vkDestroyImage(m_base.device, im.image, m_vk_alloc);
	}
	vkDestroyBuffer(m_base.device, m_res_data.staging_buffer, m_vk_alloc);
	vkDestroyBuffer(m_base.device, m_res_data.buffer, m_vk_alloc);

	vkDestroyDescriptorPool(m_base.device, m_dp, m_vk_alloc);
}

void engine::init(const base_info& info)
{
	assert(m_instance == nullptr);
	m_instance = new engine(info);
}

void engine::destroy()
{
	assert(m_instance != nullptr);
	delete m_instance;
	m_instance = nullptr;
}