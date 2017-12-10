#include "gta5_pass.h"

#include "resource_manager.h"
#include "host_memory.h"
#include "device_memory.h"
#include "gta5_foundation.h"
#include "terrain_manager.h"
#include "os_memory.h"

using namespace rcq;

gta5_pass* gta5_pass::m_instance = nullptr;

gta5_pass::gta5_pass(const base_info& info) : m_base(info)
{
	create_memory_resources_and_containers();
	create_resources();
	create_render_passes();
	create_dsls_and_allocate_dss();
	create_graphics_pipelines();
	create_compute_pipelines();
	create_command_pool();
	create_samplers();
	update_descriptor_sets();
	create_framebuffers();
	allocate_command_buffers();
	create_sync_objects();
	record_present_command_buffers();
}


gta5_pass::~gta5_pass()
{
	vkQueueWaitIdle(m_base.graphics_queue);
	vkQueueWaitIdle(m_base.compute_queue);
	vkQueueWaitIdle(m_base.present_queue);

	vkDestroyEvent(m_base.device, m_water_tex_ready_e, m_vk_alloc);
	vkDestroyFence(m_base.device, m_render_finished_f, m_vk_alloc);
	vkDestroyFence(m_base.device, m_compute_finished_f, m_vk_alloc);
	vkDestroySemaphore(m_base.device, m_render_finished_s, m_vk_alloc);
	vkDestroySemaphore(m_base.device, m_image_available_s, m_vk_alloc);
	for (auto s : m_present_ready_ss)
		vkDestroySemaphore(m_base.device, s, m_vk_alloc);

	vkDestroyFramebuffer(m_base.device, m_fbs.environment_map_gen, m_vk_alloc);
	vkDestroyFramebuffer(m_base.device, m_fbs.dir_shadow_map_gen, m_vk_alloc);
	vkDestroyFramebuffer(m_base.device, m_fbs.gbuffer_assembler, m_vk_alloc);
	vkDestroyFramebuffer(m_base.device, m_fbs.ssao_map_gen, m_vk_alloc);
	vkDestroyFramebuffer(m_base.device, m_fbs.preimage_assembler, m_vk_alloc);
	for(auto fb : m_fbs.postprocessing)
		vkDestroyFramebuffer(m_base.device, fb, m_vk_alloc);

	vkDestroyCommandPool(m_base.device, m_graphics_cp, m_vk_alloc);
	vkDestroyCommandPool(m_base.device, m_compute_cp, m_vk_alloc);
	vkDestroyCommandPool(m_base.device, m_present_cp, m_vk_alloc);

	for (auto s : m_samplers)
		vkDestroySampler(m_base.device, s, m_vk_alloc);

	for (auto& im : m_res_image)
	{
		vkDestroyImageView(m_base.device, im.view, m_vk_alloc);
		vkDestroyImage(m_base.device, im.image, m_vk_alloc);
	}

	vkUnmapMemory(m_base.device, m_res_data.staging_buffer_mem);
	vkDestroyBuffer(m_base.device, m_res_data.staging_buffer, m_vk_alloc);
	vkDestroyBuffer(m_base.device, m_res_data.buffer, m_vk_alloc);

	vkDestroyDescriptorPool(m_base.device, m_dp, m_vk_alloc);

	for (auto& gp : m_gps)
	{
		vkDestroyPipeline(m_base.device, gp.gp, m_vk_alloc);
		vkDestroyPipelineLayout(m_base.device, gp.pl, m_vk_alloc);
		vkDestroyDescriptorSetLayout(m_base.device, gp.dsl, m_vk_alloc);
	}

	for (auto& cp : m_cps)
	{
		vkDestroyPipeline(m_base.device, cp.gp, m_vk_alloc);
		vkDestroyPipelineLayout(m_base.device, cp.pl, m_vk_alloc);
		vkDestroyDescriptorSetLayout(m_base.device, cp.dsl, m_vk_alloc);
	}
	for(auto pass : m_passes)
		vkDestroyRenderPass(m_base.device, pass, m_vk_alloc);
}

void gta5_pass::create_memory_resources_and_containers()
{
	m_host_memory.init(256 * 1024 * 1024, MAX_ALIGNMENT, &OS_MEMORY);

	m_vk_alloc.init(&m_host_memory);

	m_vk_device_memory.init(m_base.device, find_memory_type(m_base.physical_device, ~0,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), &m_vk_alloc);
	m_device_memory.init(512 * 1024 * 1024, MAX_ALIGNMENT, &m_vk_device_memory, &m_host_memory);

	m_vk_mappable_memory.init(m_base.device, find_memory_type(m_base.physical_device, ~0,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), &m_vk_alloc);

	m_mappable_memory.init(1024 * 1024, MAX_ALIGNMENT, &m_vk_mappable_memory, &m_host_memory);

	m_opaque_objects.init(64, &m_host_memory);
}

void gta5_pass::init(const base_info& info, size_t place)
{
	assert(m_instance == nullptr);

	m_instance = new(reinterpret_cast<void*>(place)) gta5_pass(info);
}

void gta5_pass::destroy()
{
	assert(m_instance != nullptr);

	m_instance->~gta5_pass();
}






void gta5_pass::create_command_pool()
{
	VkCommandPoolCreateInfo pool = {};
	pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool.queueFamilyIndex = m_base.queue_families.graphics_family;
	assert(vkCreateCommandPool(m_base.device, &pool, m_vk_alloc, &m_graphics_cp) == VK_SUCCESS);

	pool.queueFamilyIndex = m_base.queue_families.present_family;
	assert(vkCreateCommandPool(m_base.device, &pool, m_vk_alloc, &m_present_cp) != VK_SUCCESS);

	pool.queueFamilyIndex = m_base.queue_families.compute_family;
	assert(vkCreateCommandPool(m_base.device, &pool, m_vk_alloc, &m_compute_cp) != VK_SUCCESS);
}

void gta5_pass::create_samplers()
{
	//general
	{
		VkSamplerCreateInfo sampler = {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.anisotropyEnable = VK_TRUE;
		sampler.compareEnable = VK_FALSE;
		sampler.unnormalizedCoordinates = VK_FALSE;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.maxAnisotropy = 16.f;
		sampler.minLod = 0.f;
		sampler.maxLod = 0.f;
		sampler.mipLodBias = 0.f;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		assert(vkCreateSampler(m_base.device, &sampler, m_vk_alloc, &m_samplers[SAMPLER_GENERAL]) == VK_SUCCESS);
	}

	//unnormalized coord
	{
		VkSamplerCreateInfo sampler = {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.anisotropyEnable = VK_FALSE;
		sampler.compareEnable = VK_FALSE;
		sampler.unnormalizedCoordinates = VK_TRUE;
		sampler.magFilter = VK_FILTER_NEAREST;
		sampler.minFilter = VK_FILTER_NEAREST;
		sampler.minLod = 0.f;
		sampler.maxLod = 0.f;
		sampler.mipLodBias = 0.f;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		assert(vkCreateSampler(m_base.device, &sampler, m_vk_alloc, &m_samplers[SAMPLER_UNNORMALIZED_COORD]) == VK_SUCCESS);
	}
}

void gta5_pass::create_sync_objects()
{
	VkSemaphoreCreateInfo s = {};
	s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	assert(vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &m_image_available_s) == VK_SUCCESS &&
		vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &m_render_finished_s) == VK_SUCCESS &&
		vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &m_bloom_blur_ready_s) == VK_SUCCESS &&
		vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &m_preimage_ready_s) == VK_SUCCESS);

	for (auto& sem : m_present_ready_ss)
		assert(vkCreateSemaphore(m_base.device, &s, m_vk_alloc, &sem) == VK_SUCCESS);

	VkFenceCreateInfo f = {};
	f.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	f.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	assert(vkCreateFence(m_base.device, &f, m_vk_alloc, &m_render_finished_f) == VK_SUCCESS &&
		vkCreateFence(m_base.device, &f, m_vk_alloc, &m_compute_finished_f) == VK_SUCCESS);

	VkEventCreateInfo e = {};
	e.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	assert(vkCreateEvent(m_base.device, &e, m_vk_alloc, &m_water_tex_ready_e) == VK_SUCCESS);

}



void gta5_pass::allocate_command_buffers()
{
	//primary cb
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = 1;
		alloc.commandPool = m_graphics_cp;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_render_cb) == VK_SUCCESS);

	}

	//present cbs
	{

		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = SWAP_CHAIN_SIZE;
		alloc.commandPool = m_graphics_cp;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, m_present_cbs) == VK_SUCCESS);
	}
		
	//compute cbs
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = 1;
		alloc.commandPool = m_compute_cp;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_terrain_request_cb) == VK_SUCCESS);
		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_water_fft_cb) == VK_SUCCESS);
		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_bloom_blur_cb) == VK_SUCCESS);
	}

	//secondary cbs
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = SECONDARY_CB_COUNT;
		alloc.commandPool = m_graphics_cp;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, m_secondary_cbs) == VK_SUCCESS);
	}
}

void gta5_pass::record_present_command_buffers()
{
	for (uint32_t i = 0; i < SWAP_CHAIN_SIZE; ++i)
	{
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		assert(vkBeginCommandBuffer(m_present_cbs[i], &begin) == VK_SUCCESS);

		VkRenderPassBeginInfo pass = {};
		pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		pass.clearValueCount = 0;
		pass.framebuffer = m_fbs.postprocessing[i];
		pass.renderPass = m_passes[RENDER_PASS_POSTPROCESSING];
		pass.renderArea.extent = m_base.swap_chain_image_extent;
		pass.renderArea.offset = { 0,0 };
		
		vkCmdBeginRenderPass(m_present_cbs[i], &pass, VK_SUBPASS_CONTENTS_INLINE);
		m_gps[GP_POSTPROCESSING].bind(m_present_cbs[i]);
		vkCmdDraw(m_present_cbs[i], 4, 1, 0, 0);
		vkCmdEndRenderPass(m_present_cbs[i]);

		assert(vkEndCommandBuffer(m_present_cbs[i]) == VK_SUCCESS);
	}

	//record bloom blur cb
	{
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		assert(vkBeginCommandBuffer(m_bloom_blur_cb, &begin) == VK_SUCCESS);

		glm::ivec2 size = { m_base.swap_chain_image_extent.width / BLOOM_IMAGE_SIZE_FACTOR, 
			m_base.swap_chain_image_extent.height / BLOOM_IMAGE_SIZE_FACTOR };

		m_cps[CP_BLOOM_BLUR].bind(m_bloom_blur_cb, VK_PIPELINE_BIND_POINT_COMPUTE);
		uint32_t step = 0;
		vkCmdPushConstants(m_bloom_blur_cb, m_cps[CP_BLOOM_BLUR].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &step);
		vkCmdDispatch(m_bloom_blur_cb, size.x, size.y, 1);

		for (uint32_t i = 0; i < 1; ++i)
		{
			VkMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(m_bloom_blur_cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 1, &b, 0, nullptr, 0, nullptr);

			step = 1;
			vkCmdPushConstants(m_bloom_blur_cb, m_cps[CP_BLOOM_BLUR].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &step);
			vkCmdDispatch(m_bloom_blur_cb, size.x, size.y, 1);

			vkCmdPipelineBarrier(m_bloom_blur_cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 1, &b, 0, nullptr, 0, nullptr);

			step = 2;
			vkCmdPushConstants(m_bloom_blur_cb, m_cps[CP_BLOOM_BLUR].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &step);
			vkCmdDispatch(m_bloom_blur_cb, size.x, size.y, 1);
		}

		//barrier for bloom blur image
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.image = m_res_image[RES_IMAGE_BLOOM_BLUR].image;
			b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 2;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(m_bloom_blur_cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}
		assert(vkEndCommandBuffer(m_bloom_blur_cb) == VK_SUCCESS);
	}
}


void gta5_pass::render(/*const render_settings & settings, std::bitset<RENDERABLE_TYPE_COUNT> record_mask*/)
{
	timer t;
	t.start();

	process_settings();

	if (m_opaque_objects.size()!=0)
	{
		auto cb = m_secondary_cbs[SECONDARY_CB_MAT_EM];

		VkCommandBufferInheritanceInfo inharitance = {};
		inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inharitance.framebuffer = m_fbs.environment_map_gen;
		inharitance.occlusionQueryEnable = VK_FALSE;
		inharitance.renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];
		inharitance.subpass = 0;

		VkCommandBufferBeginInfo begin = {};
		begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT|VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.pInheritanceInfo = &inharitance;

		assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].gp);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].pl,
			0, 1, &m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].ds, 0, nullptr);

		/*m_opaque_objects.for_each([cb, pl=m_gps[GP_ENVIRONMENT_MAP_GEN_MAT]])


		for (auto& r : m_renderables[RENDERABLE_TYPE_MAT_EM])
		{
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].pl,
				1, 1, &r.tr_ds, 0, nullptr);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].pl,
				2, 1, &r.mat_light_ds, 0, nullptr);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cb, 0, 1, &r.m.vb, &offset);
			vkCmdBindIndexBuffer(cb, r.m.ib, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cb, r.m.size, 1, 0, 0, 0);
		}*/

		assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
	}
	/*if (record_mask[RENDERABLE_TYPE_SKYBOX] && !m_renderables[RENDERABLE_TYPE_SKYBOX].empty())
	{
		auto cb = m_secondary_cbs[SECONDARY_CB_SKYBOX_EM];

		VkCommandBufferInheritanceInfo inharitance = {};
		inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inharitance.framebuffer = m_fbs.environment_map_gen;
		inharitance.occlusionQueryEnable = VK_FALSE;
		inharitance.renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];
		inharitance.subpass = 0;

		VkCommandBufferBeginInfo begin = {};
		begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.pInheritanceInfo = &inharitance;

		if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS)
			throw std::runtime_error("failed to begin command buffer!");

		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].gp);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].pl,
			0, 1, &m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].ds, 0, nullptr);

		for (auto& r : m_renderables[RENDERABLE_TYPE_SKYBOX])
		{
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_ENVIRONMENT_MAP_GEN_SKYBOX].pl,
				1, 1, &r.mat_light_ds, 0, nullptr);
			vkCmdDraw(cb, 1, 1, 0, 0);
		}

		if (vkEndCommandBuffer(cb) != VK_SUCCESS)
			throw std::runtime_error("failed to record command buffer!");
	}*/

	if (m_opaque_objects.size()!=0)
	{
		//shadow map gen
		{
			auto cb = m_secondary_cbs[SECONDARY_CB_DIR_SHADOW_GEN];

			VkCommandBufferInheritanceInfo inharitance = {};
			inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inharitance.framebuffer = m_fbs.dir_shadow_map_gen;
			inharitance.occlusionQueryEnable = VK_FALSE;
			inharitance.renderPass = m_passes[RENDER_PASS_DIR_SHADOW_MAP_GEN];
			inharitance.subpass = 0;

			VkCommandBufferBeginInfo begin = {};
			begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin.pInheritanceInfo = &inharitance;

			assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_DIR_SHADOW_MAP_GEN].gp);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_DIR_SHADOW_MAP_GEN].pl,
				0, 1, &m_gps[GP_DIR_SHADOW_MAP_GEN].ds, 0, nullptr);

			m_opaque_objects.for_each([cb, pl = m_gps[GP_DIR_SHADOW_MAP_GEN].pl](auto&& obj)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pl,
					1, 1, &obj.tr_ds, 0, nullptr);
				VkDeviceSize offset = 0;
				vkCmdBindVertexBuffers(cb, 0, 1, &obj.mesh_vb, &offset);
				vkCmdBindIndexBuffer(cb, obj.mesh_ib, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cb, obj.mesh_index_size, 1, 0, 0, 0);
			});

			assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
		}

		//gbuffer gen
		{
			auto cb = m_secondary_cbs[SECONDARY_CB_MAT_OPAQUE];

			VkCommandBufferInheritanceInfo inharitance = {};
			inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inharitance.framebuffer = m_fbs.gbuffer_assembler;
			inharitance.occlusionQueryEnable = VK_FALSE;
			inharitance.renderPass = m_passes[RENDER_PASS_GBUFFER_ASSEMBLER];
			inharitance.subpass = render_pass_gbuffer_assembler::SUBPASS_GBUFFER_GEN;

			VkCommandBufferBeginInfo begin = {};
			begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin.pInheritanceInfo = &inharitance;

			assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_GBUFFER_GEN].gp);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_GBUFFER_GEN].pl,
				0, 1, &m_gps[GP_GBUFFER_GEN].ds, 0, nullptr);

			m_opaque_objects.for_each([cb, pl = m_gps[GP_GBUFFER_GEN].pl](auto&& obj)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,pl,
					1, 1, &obj.tr_ds, 0, nullptr);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pl,
					2, 1, &obj.opaque_material_ds, 0, nullptr);
				std::array<VkBuffer, 2> vertex_buffers = { obj.mesh_vb, obj.mesh_veb };
				if (vertex_buffers[1] == VK_NULL_HANDLE)
					vertex_buffers[1] = obj.mesh_vb;
				std::array<VkDeviceSize, 2> offsets = { 0,0 };
				vkCmdBindVertexBuffers(cb, 0, 2, vertex_buffers.data(), offsets.data());
				vkCmdBindIndexBuffer(cb, obj.mesh_ib, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cb, obj.mesh_index_size, 1, 0, 0, 0);
			});

			assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
		}
	}

	vkWaitForFences(m_base.device, 1, &m_render_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_base.device, 1, &m_render_finished_f);
	vkResetEvent(m_base.device, m_water_tex_ready_e);

	//manage terrain request
	if (m_terrain_valid)
	{
		terrain_manager::instance()->poll_results();
		vkWaitForFences(m_base.device, 1, &m_compute_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(m_base.device, 1, &m_compute_finished_f);
		terrain_manager::instance()->poll_requests();

		std::array<VkCommandBuffer, 2 > cbs = { m_terrain_request_cb, m_water_fft_cb };
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = cbs.size();
		submit.pCommandBuffers = cbs.data();

		std::lock_guard<std::mutex> lock(m_base.compute_queue_mutex);
		vkQueueSubmit(m_base.compute_queue, 1, &submit, m_compute_finished_f);
	}

	//record primary cb
	{
		auto cb = m_render_cb;
		//vkResetCommandBuffer(cb, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

		VkCommandBufferBeginInfo cb_begin = {};
		cb_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cb_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		assert(vkBeginCommandBuffer(cb, &cb_begin) == VK_SUCCESS);

		//copy data from staging buffer
		{
			VkBufferCopy region = {};
			region.dstOffset = 0;
			region.srcOffset = 0;
			region.size = m_res_data.size;

			vkCmdCopyBuffer(cb, m_res_data.staging_buffer, m_res_data.buffer, 1, &region);

			VkBufferMemoryBarrier barrier = {};
			barrier.buffer = m_res_data.buffer;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.offset = 0;
			barrier.size = m_res_data.size;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
			barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr,
				1, &barrier, 0, nullptr);
		}

		//environment map gen
		/*if(!m_renderables[RENDERABLE_TYPE_MAT_EM].empty() && !m_renderables[RENDERABLE_TYPE_SKYBOX].empty())
		{
			using namespace render_pass_environment_map_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.environment_map_gen;
			begin.renderPass = m_passes[RENDER_PASS_ENVIRONMENT_MAP_GEN];

			std::array<VkClearValue, ATT_COUNT> clears = {};
			clears[ATT_DEPTH].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT_COUNT;
			begin.pClearValues = clears.data();
			begin.renderArea.extent.width = ENVIRONMENT_MAP_SIZE;
			begin.renderArea.extent.height = ENVIRONMENT_MAP_SIZE;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			//vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_MAT_EM]);
			//vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_SKYBOX_EM]);
			//vkCmdEndRenderPass(cb);

			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_ENVIRONMENT_MAP].image;
			b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 6;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}*/

		//dir shadow map gen
		if (m_opaque_objects.size()!=0)
		{
			using namespace render_pass_dir_shadow_map_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.dir_shadow_map_gen;
			begin.renderPass = m_passes[RENDER_PASS_DIR_SHADOW_MAP_GEN];

			std::array<VkClearValue, ATT_COUNT> clears = {};
			clears[ATT_DEPTH].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT_COUNT;
			begin.pClearValues = clears.data();
			begin.renderArea.extent.width = DIR_SHADOW_MAP_SIZE;
			begin.renderArea.extent.height = DIR_SHADOW_MAP_SIZE;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_DIR_SHADOW_GEN]);
			vkCmdEndRenderPass(cb);

			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image;
			b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = FRUSTUM_SPLIT_COUNT;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//gbuffer assembler
		{
			using namespace render_pass_gbuffer_assembler;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.gbuffer_assembler;
			begin.renderPass = m_passes[RENDER_PASS_GBUFFER_ASSEMBLER];

			std::array<VkClearValue, ATT_COUNT> clears = {};
			clears[ATT_DEPTHSTENCIL].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT_COUNT;
			begin.pClearValues = clears.data();
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			if(m_opaque_objects.size()!=0)
				vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_MAT_OPAQUE]);

			if (m_terrain_valid)
			{
				vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_TERRAIN_DRAWER]);
			}

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SS_DIR_SHADOW_MAP_GEN].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdEndRenderPass(cb);
		}

		//barrier for ssds map
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_DIR_SHADOW_MAP].image;
			b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}
		//barrier for pos and normal images
		{
			VkImageMemoryBarrier bs[2] = {};
			for (auto& b : bs)
			{
				b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				b.subresourceRange.baseArrayLayer = 0;
				b.subresourceRange.baseMipLevel = 0;
				b.subresourceRange.layerCount = 1;
				b.subresourceRange.levelCount = 1;
			}
			bs[0].image = m_res_image[RES_IMAGE_GB_POS_ROUGHNESS].image;
			bs[1].image = m_res_image[RES_IMAGE_GB_NORMAL_AO].image;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 2, bs);
		}

		//barrier for basecolor_ssao and metalness_ssds
		{
			std::array<VkImageMemoryBarrier, 2> bs = {};
			for (auto& b : bs)
			{
				b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				b.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
				b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				b.subresourceRange.baseArrayLayer = 0;
				b.subresourceRange.baseMipLevel = 0;
				b.subresourceRange.layerCount = 1;
				b.subresourceRange.levelCount = 1;
			}
			bs[0].image = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image;
			bs[1].image = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0, 0, nullptr, 0, nullptr, 2, bs.data());
		}

		//barrier for depthstencil
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_GB_DEPTH].image;
			b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//ssao map gen
		{
			using namespace render_pass_ssao_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.ssao_map_gen;
			begin.renderPass = m_passes[RENDER_PASS_SSAO_MAP_GEN];
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SSAO_GEN].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);
			vkCmdEndRenderPass(cb);
		}

		//barrier for ssao map
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;
			b.image = m_res_image[RES_IMAGE_SSAO_MAP].image;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//preimage assembler
		{
			using namespace render_pass_preimage_assembler;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.preimage_assembler;
			begin.renderPass = m_passes[RENDER_PASS_PREIMAGE_ASSEMBLER];
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SSAO_BLUR].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SSR_RAY_CASTING].bind(cb);
			vkCmdDraw(cb, m_base.swap_chain_image_extent.width, m_base.swap_chain_image_extent.height, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_IMAGE_ASSEMBLER].bind(cb);
			if (m_sky_valid)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_IMAGE_ASSEMBLER].pl,
					1, 1, &m_sky.ds, 0, nullptr);
			}
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SKY_DRAWER].bind(cb);
			if (m_sky_valid)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_SKY_DRAWER].pl,
					1, 1, &m_sky.ds, 0, nullptr);
				vkCmdDraw(cb, 1, 1, 0, 0);
			}

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SUN_DRAWER].bind(cb);
			if (m_sky_valid)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_SUN_DRAWER].pl,
					1, 1, &m_sky.ds, 0, nullptr);
				vkCmdDraw(cb, 18, 1, 0, 0);
			}

			vkCmdEndRenderPass(cb);
		}

		/*//barrier for preimage
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_PREIMAGE].image;
			b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}*/

		//refraction map gen
		{
			using namespace render_pass_refraction_image_gen;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.clearValueCount = ATT_COUNT;
			std::array<VkClearValue, ATT_COUNT> clears;
			clears[ATT_REFRACTION_IMAGE].color = { 0.f, 0.f, 0.f, 0.f };
			begin.pClearValues = clears.data();
			begin.framebuffer = m_fbs.refraction_image_gen;
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };
			begin.renderPass = m_passes[RENDER_PASS_REFRACTION_IMAGE_GEN];

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_REFRACTION_IMAGE_GEN].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);
			vkCmdEndRenderPass(cb);
		}

		//barrier for refraction image
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_REFRACTION_IMAGE].image;
			b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//water
		{
			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs.water;
			begin.renderArea.extent = m_base.swap_chain_image_extent;
			begin.renderArea.offset = { 0,0 };
			begin.renderPass = m_passes[RENDER_PASS_WATER];

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_INLINE);
			
			m_gps[GP_WATER_DRAWER].bind(cb);

			if (m_water_valid)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_WATER_DRAWER].pl,
					1, 1, &m_water.ds, 0, nullptr);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_WATER_DRAWER].pl,
					2, 1, &m_sky.ds, 0, nullptr);

				vkCmdDraw(cb, m_water_tiles_count.x * 4, m_water_tiles_count.y, 0, 0);
			}

			vkCmdEndRenderPass(cb);
		}

		/*//barrier for preimage
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_PREIMAGE].image;
			b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}*/

		//transition bloom blur image
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_BLOOM_BLUR].image;
			b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			b.srcAccessMask = 0;
			b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 2;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//blit preimage to bloom blur
		{
			VkImageBlit blit = {};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { 
				static_cast<int32_t>(m_base.swap_chain_image_extent.width),
				static_cast<int32_t>(m_base.swap_chain_image_extent.height), 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.srcSubresource.mipLevel = 0;

			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { 
				static_cast<int32_t>(m_base.swap_chain_image_extent.width / BLOOM_IMAGE_SIZE_FACTOR),
				static_cast<int32_t>(m_base.swap_chain_image_extent.height / BLOOM_IMAGE_SIZE_FACTOR), 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;
			blit.dstSubresource.mipLevel = 0;

			vkCmdBlitImage(cb, m_res_image[RES_IMAGE_PREIMAGE].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				m_res_image[RES_IMAGE_BLOOM_BLUR].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
		}

		//transition bloom blur
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_BLOOM_BLUR].image;
			b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 2;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//barrier for preimage
		{
			VkImageMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.image = m_res_image[RES_IMAGE_PREIMAGE].image;
			b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}


		assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
	}

	t.stop();
	//std::cout << "render_function_time: " << t.get() << std::endl;

	uint32_t image_index;
	vkAcquireNextImageKHR(m_base.device, m_base.swap_chain, std::numeric_limits<uint64_t>::max(), m_image_available_s,
		VK_NULL_HANDLE, &image_index);

	//submit render_buffer
	{
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_render_cb;
		std::array<VkSemaphore, 2> signal_s = { m_render_finished_s, m_preimage_ready_s };
		submit.pSignalSemaphores = signal_s.data();
		submit.signalSemaphoreCount = 2;
		
		vkWaitForFences(m_base.device, 1, &m_compute_finished_f, VK_TRUE, std::numeric_limits<uint64_t>::max());
		//vkSetEvent(m_base.device, m_water_tex_ready_e);
		
		std::lock_guard<std::mutex> lock(m_base.graphics_queue_mutex);
		assert(vkQueueSubmit(m_base.graphics_queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
	}

	//submit bloom blur
	{
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_bloom_blur_cb;
		submit.pSignalSemaphores = &m_bloom_blur_ready_s;
		submit.signalSemaphoreCount = 1;
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &m_preimage_ready_s;
		VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		submit.pWaitDstStageMask = &wait;

		std::lock_guard<std::mutex> lock(m_base.compute_queue_mutex);
		assert(vkQueueSubmit(m_base.compute_queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
	}

	//submit postprocessing
	{
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_present_cbs[image_index];

		std::array<VkSemaphore, 3> wait_ss = { m_image_available_s, m_render_finished_s, m_bloom_blur_ready_s };
		submit.pWaitSemaphores = wait_ss.data();
		submit.waitSemaphoreCount = wait_ss.size();
		std::array<VkPipelineStageFlags, 3> waits = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		submit.pWaitDstStageMask = waits.data();
		submit.pSignalSemaphores = &m_present_ready_ss[image_index];
		submit.signalSemaphoreCount = 1;

		std::lock_guard<std::mutex> lock(m_base.graphics_queue_mutex);
		assert(vkQueueSubmit(m_base.graphics_queue, 1, &submit, m_render_finished_f) == VK_SUCCESS);
	}

	//present swap chain image
	{
		VkPresentInfoKHR present = {};
		present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present.pImageIndices = &image_index;
		present.pSwapchains = &m_base.swap_chain;
		present.swapchainCount = 1;
		present.pWaitSemaphores = &m_present_ready_ss[image_index];
		present.waitSemaphoreCount = 1;
		
		assert(vkQueuePresentKHR(m_base.present_queue, &present) == VK_SUCCESS);
	}

}

void gta5_pass::process_settings()
{
	auto projs = calc_projs();
	float height_bias = 1000.f;
	float height = height_bias+m_render_settings.pos.y;

	//environment map gen mat
	{
		auto data = m_res_data.get<RES_DATA_ENVIRONMENT_MAP_GEN_MAT>();
		data->ambient_irradiance = m_render_settings.ambient_irradiance;
		data->dir = m_render_settings.light_dir;
		data->irradiance = m_render_settings.irradiance;
		data->view = m_render_settings.view;
	}

	//environment map gen skybox
	{
		auto data = m_res_data.get<RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX>();
		data->view = m_render_settings.view;
	}

	//dir shadow map gen
	{
		auto data = m_res_data.get<RES_DATA_DIR_SHADOW_MAP_GEN>();
		auto projs = calc_projs();
		std::array<glm::mat4, projs.size()> projs_from_world;
		for (uint32_t i = 0; i < projs.size(); ++i)
			projs_from_world[i] = projs[i] * m_render_settings.view;
		memcpy(data->projs, projs_from_world.data(), sizeof(glm::mat4)*projs.size());
	}

	//gbuffer gen
	{
		auto data = m_res_data.get<RES_DATA_GBUFFER_GEN>();
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->cam_pos = m_render_settings.pos;
	}

	//ss dir shadow map gen
	{
		auto data = m_res_data.get<RES_DATA_SS_DIR_SHADOW_MAP_GEN>();
		data->light_dir = m_render_settings.light_dir;
		data->view = m_render_settings.view;
		data->far = m_render_settings.far;
		data->near = m_render_settings.near;
		memcpy(data->projs, projs.data(), sizeof(glm::mat4)*projs.size());
	}

	//image assembler data
	{
		auto data = m_res_data.get<RES_DATA_IMAGE_ASSEMBLER>();
		data->ambient_irradiance = m_render_settings.ambient_irradiance;
		data->dir = m_render_settings.light_dir;
		data->irradiance = m_render_settings.irradiance;
		data->cam_pos = m_render_settings.pos;
		data->height_bias = height_bias;
		data->previous_proj_x_view = m_previous_proj_x_view;
		data->previous_view_pos = m_previous_view_pos;
	}

	glm::mat4 view_at_origin = m_render_settings.view;
	view_at_origin[3][0] = 0.f;
	view_at_origin[3][1] = 0.f;
	view_at_origin[3][2] = 0.f;
	glm::mat4 proj_x_view_at_origin = m_render_settings.proj*view_at_origin;

	//sky drawer data
	{
		auto data = m_res_data.get<RES_DATA_SKY_DRAWER>();
		
		data->proj_x_view_at_origin = proj_x_view_at_origin;
		data->height = height;
		data->irradiance = m_render_settings.irradiance;
		data->light_dir = m_render_settings.light_dir;
	}

	//sun drawer data
	{
		auto data = m_res_data.get<RES_DATA_SUN_DRAWER>();
		data->height = height;
		data->irradiance = m_render_settings.irradiance;
		data->light_dir = m_render_settings.light_dir;
		data->proj_x_view_at_origin = proj_x_view_at_origin;
		data->helper_dir = get_orthonormal(m_render_settings.light_dir);
	}

	//terrain drawer data
	{
		auto data = m_res_data.get<RES_DATA_TERRAIN_DRAWER>();
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->view_pos = m_render_settings.pos;
	}

	//terrain tile request data
	{
		auto data = m_res_data.get<RES_DATA_TERRAIN_TILE_REQUEST>();
		data->far = m_render_settings.far;
		data->near = m_render_settings.near;
		data->view_pos = m_render_settings.pos;
	}

	//water drawer data
	{
		auto data = m_res_data.get<RES_DATA_WATER_DRAWER>();
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->half_resolution.x = static_cast<float>(m_base.swap_chain_image_extent.width) / 2.f;
		data->half_resolution.y = static_cast<float>(m_base.swap_chain_image_extent.height) / 2.f;
		data->light_dir = m_render_settings.light_dir;
		data->view_pos = m_render_settings.pos;
		data->tile_size_in_meter = m_water.grid_size_in_meters;
		data->tile_offset = (glm::floor(glm::vec2(m_render_settings.pos.x - m_render_settings.far, m_render_settings.pos.z - m_render_settings.far)
			/ m_water.grid_size_in_meters)+glm::vec2(0.5f))*
			m_water.grid_size_in_meters;
		m_water_tiles_count = static_cast<glm::uvec2>(glm::ceil(glm::vec2(2.f*m_render_settings.far) / m_water.grid_size_in_meters));
		data->ambient_irradiance = m_render_settings.ambient_irradiance;
		data->height_bias = height_bias;
		data->mirrored_proj_x_view = glm::mat4(1.f); //CORRECT IT!!!
		data->irradiance = m_render_settings.irradiance;
	}

	//refraction map gen
	{
		auto data = m_res_data.get<RES_DATA_REFRACTION_MAP_GEN>();
		data->far = m_render_settings.far;
		data->proj_x_view = m_render_settings.proj*m_render_settings.view;
		data->view_pos_at_ground = { m_render_settings.pos.x, glm::min(0.05f*glm::dot(m_render_settings.wind, m_render_settings.wind) / 9.81f,
			m_render_settings.pos.y - m_render_settings.near - 0.000001f), m_render_settings.pos.z };
	}

	//ssr ray casting
	{
		auto data = m_res_data.get<RES_DATA_SSR_RAY_CASTING>();
		data->proj_x_view= m_render_settings.proj*m_render_settings.view;
		data->ray_length = glm::length(glm::vec3(m_render_settings.far / m_render_settings.proj[0][0], m_render_settings.far /
			m_render_settings.proj[1][1], m_render_settings.far));
		data->view_pos = m_render_settings.pos;
	}

	//water compute data
	{
		auto data = m_res_data.get<RES_DATA_WATER_COMPUTE>();
		data->one_over_wind_speed_to_the_4 = 1.f / powf(glm::length(m_render_settings.wind), 4.f);
		data->wind_dir = glm::normalize(m_render_settings.wind);
		data->time = m_render_settings.time;
	}

	m_previous_proj_x_view = m_render_settings.proj*m_render_settings.view;
	m_previous_view_pos = m_render_settings.pos;
}

std::array<glm::mat4, FRUSTUM_SPLIT_COUNT> gta5_pass::calc_projs()
{
	std::array<glm::mat4, FRUSTUM_SPLIT_COUNT> projs;

	glm::vec3 light_dir = static_cast<glm::mat3>(m_render_settings.view)*m_render_settings.light_dir;
	glm::mat4 light = glm::lookAtLH(glm::vec3(0.f), light_dir, glm::vec3(0.f, 1.f, 0.f));
	std::array<std::array<glm::vec3, 4>, FRUSTUM_SPLIT_COUNT + 1> frustum_points;
	
	glm::vec2 max[2];
	glm::vec2 min[2];

	for (uint32_t i = 0; i < FRUSTUM_SPLIT_COUNT + 1; ++i)
	{
		float z = m_render_settings.near + std::powf(static_cast<float>(i) / static_cast<float>(FRUSTUM_SPLIT_COUNT), 3.f)*
			(m_render_settings.far - m_render_settings.near);		
		z *= (-1.f);

		std::array<float, 4> x = { -1.f, 1.f, -1.f, 1.f };
		std::array<float, 4> y = { -1.f, -1.f, 1.f, 1.f };

		max[i % 2] = glm::vec2(std::numeric_limits<float>::min());
		min[i % 2] = glm::vec2(std::numeric_limits<float>::max());

		for (uint32_t j=0; j<4; ++j)
		{
			glm::vec3 v = { x[j]*z / m_render_settings.proj[0][0], y[j]*z / m_render_settings.proj[1][1], z };
			v = static_cast<glm::mat3>(light)*v;
			frustum_points[i][j] = v;

			max[i % 2] = componentwise_max(max[i % 2], { v.x, v.y });
			min[i % 2] = componentwise_min(min[i % 2], { v.x, v.y });
		}

		if (i > 0)
		{
			glm::vec2 frustum_max = componentwise_max(max[0], max[1]);
			glm::vec2 frustum_min = componentwise_min(min[0], min[1]);

			glm::vec2 center = (frustum_max + frustum_min)*0.5f;
			glm::vec2 scale = glm::vec2(2.f)/(frustum_max - frustum_min);

			glm::mat4 proj(1.f);
			proj[0][0] = scale.x;
			proj[1][1] = scale.y;
			proj[2][2] = 0.5f / 1000.f;
			proj[3][0] = -scale.x*center.x;
			proj[3][1] = -scale.y*center.y;
			proj[3][2] = 0.5f;

			projs[i - 1] = proj*light;
		}
	}
	return projs; //from view space
}

void gta5_pass::set_terrain(base_resource* terrain)
{
	auto t = reinterpret_cast<resource<RES_TYPE_TERRAIN>*>(terrain->data);

	m_terrain.ds = t->ds;
	m_terrain.request_ds = t->request_ds;
	terrain_manager::instance()->set_terrain(t);

	//record terrain request cb
	{
		auto cb = m_terrain_request_cb;
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = 0;

		assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

		m_cps[CP_TERRAIN_TILE_REQUEST].bind(cb, VK_PIPELINE_BIND_POINT_COMPUTE);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_cps[CP_TERRAIN_TILE_REQUEST].pl,
			1, 1, &m_terrain.request_ds, 0, nullptr);

		glm::uvec2 group_count = t->tile_count;
		vkCmdDispatch(cb, group_count.x, group_count.y, 1);

		assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
	}

	//record terrain drawer cb
	{
		VkCommandBufferInheritanceInfo inheritance = {};
		inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritance.framebuffer = m_fbs.gbuffer_assembler;
		inheritance.occlusionQueryEnable = VK_FALSE;
		inheritance.renderPass = m_passes[RENDER_PASS_GBUFFER_ASSEMBLER];
		inheritance.subpass = render_pass_gbuffer_assembler::SUBPASS_GBUFFER_GEN;

		auto cb = m_secondary_cbs[SECONDARY_CB_TERRAIN_DRAWER];
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.pInheritanceInfo = &inheritance;
		begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);
		m_gps[GP_TERRAIN_DRAWER].bind(cb);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_TERRAIN_DRAWER].pl,
			1, 1, &m_terrain.ds, 0, nullptr);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_TERRAIN_DRAWER].pl,
			2, 4, m_terrain.opaque_material_dss.data(),
			0, nullptr);
		vkCmdDraw(cb, 4 * t->tile_count.x,
			t->tile_count.y, 0, 0);

		assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
	}

	m_terrain_valid = true;
}

void gta5_pass::set_water(base_resource* water)
{
	auto w = reinterpret_cast<resource<RES_TYPE_WATER>*>(water->data);
	m_water.ds = w->ds;
	m_water.fft_ds = w->fft_ds;
	m_water.grid_size_in_meters = w->grid_size_in_meters;

	//record water fft cb
	auto cb = m_water_fft_cb;

	VkCommandBufferBeginInfo b = {};
	b.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	
	assert(vkBeginCommandBuffer(cb, &b) == VK_SUCCESS);

	//acquire barrier
	{
		VkImageMemoryBarrier b = {};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.image = w->tex.image;
		b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 2;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
	}

	m_cps[CP_WATER_FFT].bind(cb, VK_PIPELINE_BIND_POINT_COMPUTE);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_cps[CP_WATER_FFT].pl,
		1, 1, &w->fft_ds, 0, nullptr);
	
	uint32_t fft_axis = 0;
	vkCmdPushConstants(cb, m_cps[CP_WATER_FFT].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &fft_axis);
	vkCmdDispatch(cb, 1, 1024, 1);

	//memory barrier
	{
		VkImageMemoryBarrier b = {};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		b.image = w->tex.image;
		b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.layerCount = 2;
		b.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &b);
	}	

	fft_axis = 1;
	vkCmdPushConstants(cb, m_cps[CP_WATER_FFT].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &fft_axis);
	vkCmdDispatch(cb, 1, 1024, 1);

	//release barrier
	{
		VkImageMemoryBarrier b = {};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.image = w->tex.image;
		b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 2;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &b);
	}

	assert(vkEndCommandBuffer(cb) == VK_SUCCESS);

	bool m_water_valid = true;
}