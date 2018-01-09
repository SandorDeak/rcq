#include "engine.h"

#include "rps.h"
#include "terrain_manager.h"

#include "enum_res_image.h"

#include "const_dir_shadow_map_size.h"
#include "const_swap_chain_image_extent.h"
#include "const_bloom_image_size_factor.h"
#include "const_environment_map_size.h"

using namespace rcq;

void engine::record_and_submit()
{
	if (m_opaque_objects.size() != 0)
	{
		auto cb = m_secondary_cbs[SECONDARY_CB_MAT_EM];

		VkCommandBufferInheritanceInfo inharitance = {};
		inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inharitance.framebuffer = m_fbs[FB_ENVIRONMENT_MAP_GEN];
		inharitance.occlusionQueryEnable = VK_FALSE;
		inharitance.renderPass = m_rps[RP_ENVIRONMENT_MAP_GEN];
		inharitance.subpass = 0;

		VkCommandBufferBeginInfo begin = {};
		begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.pInheritanceInfo = &inharitance;

		assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_ENVIRONMENT_MAP_GEN_MAT].ppl);
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

	if (m_opaque_objects.size() != 0)
	{
		//shadow map gen
		{
			auto cb = m_secondary_cbs[SECONDARY_CB_DIR_SHADOW_GEN];

			VkCommandBufferInheritanceInfo inharitance = {};
			inharitance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inharitance.framebuffer = m_fbs[FB_DIR_SHADOW_MAP_GEN];
			inharitance.occlusionQueryEnable = VK_FALSE;
			inharitance.renderPass = m_rps[RP_DIR_SHADOW_MAP_GEN];
			inharitance.subpass = 0;

			VkCommandBufferBeginInfo begin = {};
			begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin.pInheritanceInfo = &inharitance;

			assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_DIR_SHADOW_MAP_GEN].ppl);
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
			inharitance.framebuffer = m_fbs[FB_GBUFFER_ASSEMBLER];
			inharitance.occlusionQueryEnable = VK_FALSE;
			inharitance.renderPass = m_rps[RP_GBUFFER_ASSEMBLER];
			inharitance.subpass = rp_create_info<RP_GBUFFER_ASSEMBLER>::SUBPASS_GBUFFER_GEN;

			VkCommandBufferBeginInfo begin = {};
			begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin.pInheritanceInfo = &inharitance;

			assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_OPAQUE_OBJ_DRAWER].ppl);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_OPAQUE_OBJ_DRAWER].pl,
				0, 1, &m_gps[GP_OPAQUE_OBJ_DRAWER].ds, 0, nullptr);

			VkPipelineLayout pl = m_gps[GP_OPAQUE_OBJ_DRAWER].pl;
			m_opaque_objects.for_each([cb, pl](auto&& obj)
			{
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pl,
					1, 1, &obj.tr_ds, 0, nullptr);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pl,
					2, 1, &obj.mat_opaque_ds, 0, nullptr);
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

	vkWaitForFences(m_base.device, 1, &m_fences[FENCE_RENDER_FINISHED], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_base.device, 1, &m_fences[FENCE_RENDER_FINISHED]);
	vkResetEvent(m_base.device, m_events[EVENT_WATER_READY]);

	//manage terrain request
	if (m_terrain_valid)
	{
		terrain_manager::instance()->poll_results();
		vkWaitForFences(m_base.device, 1, &m_fences[FENCE_COMPUTE_FINISHED], VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(m_base.device, 1, &m_fences[FENCE_COMPUTE_FINISHED]);
		terrain_manager::instance()->poll_requests();

		VkCommandBuffer cbs[2] = { m_cbs[CB_TERRAIN_REQUEST], m_cbs[CB_WATER_FFT] };
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 2;
		submit.pCommandBuffers = cbs;

		vkQueueSubmit(m_base.queues[QUEUE_COMPUTE], 1, &submit, m_fences[FENCE_COMPUTE_FINISHED]);
	}

	//record primary cb
	{
		auto cb = m_cbs[CB_RENDER];
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
		{
			using ATT = rp_create_info<RP_ENVIRONMENT_MAP_GEN>::ATT;


			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs[FB_ENVIRONMENT_MAP_GEN];
			begin.renderPass = m_rps[RP_ENVIRONMENT_MAP_GEN];

			VkClearValue clears[ATT::ATT_COUNT] = {};
			clears[ATT::ATT_DEPTH].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT::ATT_COUNT;
			begin.pClearValues = clears;
			begin.renderArea.extent.width = ENVIRONMENT_MAP_SIZE;
			begin.renderArea.extent.height = ENVIRONMENT_MAP_SIZE;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_MAT_EM]);
			//vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_SKYBOX_EM]);
			vkCmdEndRenderPass(cb);

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
		}

		//dir shadow map gen
		if (m_opaque_objects.size() != 0)
		{
			using ATT = rp_create_info<RP_DIR_SHADOW_MAP_GEN>::ATT;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs[FB_DIR_SHADOW_MAP_GEN];
			begin.renderPass = m_rps[RP_DIR_SHADOW_MAP_GEN];

			VkClearValue clears[ATT::ATT_COUNT] = {};
			clears[ATT::ATT_DEPTH].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT::ATT_COUNT;
			begin.pClearValues = clears;
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
			using ATT = rp_create_info<RP_GBUFFER_ASSEMBLER>::ATT;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs[FB_GBUFFER_ASSEMBLER];
			begin.renderPass = m_rps[RP_GBUFFER_ASSEMBLER];

			VkClearValue clears[ATT::ATT_COUNT] = {};
			clears[ATT::ATT_DEPTHSTENCIL].depthStencil = { 1.f, 0 };

			begin.clearValueCount = ATT::ATT_COUNT;
			begin.pClearValues = clears;
			begin.renderArea.extent = SWAP_CHAIN_IMAGE_EXTENT;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			if (m_opaque_objects.size() != 0)
				vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_MAT_OPAQUE]);

			if (m_terrain_valid)
			{
				//vkCmdExecuteCommands(cb, 1, &m_secondary_cbs[SECONDARY_CB_TERRAIN_DRAWER]);
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
			VkImageMemoryBarrier bs[2] = {};
			for (auto& b : bs)
			{
				b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				b.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
				b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				b.subresourceRange.baseArrayLayer = 0;
				b.subresourceRange.baseMipLevel = 0;
				b.subresourceRange.layerCount = 1;
				b.subresourceRange.levelCount = 1;
			}
			bs[0].image = m_res_image[RES_IMAGE_GB_BASECOLOR_SSAO].image;
			bs[1].image = m_res_image[RES_IMAGE_GB_METALNESS_SSDS].image;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0, 0, nullptr, 0, nullptr, 2, bs);
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
			b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			b.subresourceRange.baseArrayLayer = 0;
			b.subresourceRange.baseMipLevel = 0;
			b.subresourceRange.layerCount = 1;
			b.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}

		//ssao map gen
		{
			using ATT = rp_create_info<RP_SSAO_MAP_GEN>::ATT;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs[FB_SSAO_MAP_GEN];
			begin.renderPass = m_rps[RP_SSAO_MAP_GEN];
			begin.renderArea.extent = SWAP_CHAIN_IMAGE_EXTENT;
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
			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.framebuffer = m_fbs[FB_PREIMAGE_ASSEMBLER];
			begin.renderPass = m_rps[RP_PREIMAGE_ASSEMBLER];
			begin.renderArea.extent = SWAP_CHAIN_IMAGE_EXTENT;
			begin.renderArea.offset = { 0,0 };

			vkCmdBeginRenderPass(cb, &begin, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SS_DIR_SHADOW_MAP_BLUR].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SSAO_BLUR].bind(cb);
			vkCmdDraw(cb, 4, 1, 0, 0);

			vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
			m_gps[GP_SSR_RAY_CASTING].bind(cb);
			vkCmdDraw(cb, SWAP_CHAIN_IMAGE_EXTENT.width, SWAP_CHAIN_IMAGE_EXTENT.height, 0, 0);

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

		//barrier for preimage
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
		}

		//refraction image gen
		{
			using ATT = rp_create_info<RP_REFRACTION_IMAGE_GEN>::ATT;

			VkRenderPassBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin.clearValueCount = ATT::ATT_COUNT;
			VkClearValue clears[ATT::ATT_COUNT];
			clears[ATT::ATT_REFRACTION_IMAGE].color = { 0.f, 0.f, 0.f, 0.f };
			begin.pClearValues = clears;
			begin.framebuffer = m_fbs[FB_REFRACTION_IMAGE_GEN];
			begin.renderArea.extent = SWAP_CHAIN_IMAGE_EXTENT;
			begin.renderArea.offset = { 0,0 };
			begin.renderPass = m_rps[RP_REFRACTION_IMAGE_GEN];

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
			begin.framebuffer = m_fbs[FB_WATER_DRAWER];
			begin.renderArea.extent = SWAP_CHAIN_IMAGE_EXTENT;
			begin.renderArea.offset = { 0,0 };
			begin.renderPass = m_rps[RP_WATER_DRAWER];

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

		//barrier for preimage
		/*{
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
				static_cast<int32_t>(SWAP_CHAIN_IMAGE_EXTENT.width),
				static_cast<int32_t>(SWAP_CHAIN_IMAGE_EXTENT.height), 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.srcSubresource.mipLevel = 0;

			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = {
				static_cast<int32_t>(SWAP_CHAIN_IMAGE_EXTENT.width / BLOOM_IMAGE_SIZE_FACTOR),
				static_cast<int32_t>(SWAP_CHAIN_IMAGE_EXTENT.height / BLOOM_IMAGE_SIZE_FACTOR), 1 };
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

	uint32_t image_index;
	vkAcquireNextImageKHR(m_base.device, m_base.swapchain, std::numeric_limits<uint64_t>::max(), 
		m_semaphores[SEMAPHORE_IMAGE_AVAILABLE], VK_NULL_HANDLE, &image_index);

	//submit render_buffer
	{
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_cbs[CB_RENDER];
		VkSemaphore signal_s[2] = { m_semaphores[SEMAPHORE_RENDER_FINISHED],
			m_semaphores[SEMAPHORE_PREIMAGE_READY] };
		submit.pSignalSemaphores = signal_s;
		submit.signalSemaphoreCount = 2;

		vkWaitForFences(m_base.device, 1, &m_fences[FENCE_COMPUTE_FINISHED], VK_TRUE, std::numeric_limits<uint64_t>::max());
		//vkSetEvent(m_base.device, m_water_tex_ready_e);

		assert(vkQueueSubmit(m_base.queues[QUEUE_RENDER], 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
	}

	//submit bloom blur
	{
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_cbs[CB_BLOOM];
		submit.pSignalSemaphores = &m_semaphores[SEMAPHORE_BLOOM_READY];
		submit.signalSemaphoreCount = 1;
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &m_semaphores[SEMAPHORE_PREIMAGE_READY];
		VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		submit.pWaitDstStageMask = &wait;

		assert(vkQueueSubmit(m_base.queues[QUEUE_COMPUTE], 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
	}

	//submit postprocessing
	{
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_present_cbs[image_index];

		VkSemaphore wait_ss[3] =
		{ 
			m_semaphores[SEMAPHORE_IMAGE_AVAILABLE], 
			m_semaphores[SEMAPHORE_RENDER_FINISHED], 
			m_semaphores[SEMAPHORE_BLOOM_READY] 
		};
		submit.pWaitSemaphores = wait_ss;
		submit.waitSemaphoreCount = 3;
		VkPipelineStageFlags waits[3] = 
		{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		};
		submit.pWaitDstStageMask = waits;
		submit.pSignalSemaphores = &m_present_ready_ss[image_index];
		submit.signalSemaphoreCount = 1;

		assert(vkQueueSubmit(m_base.queues[QUEUE_RENDER], 1, &submit, m_fences[FENCE_RENDER_FINISHED]) == VK_SUCCESS);
	}

	//present swap chain image
	{
		VkPresentInfoKHR present = {};
		present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present.pImageIndices = &image_index;
		present.pSwapchains = &m_base.swapchain;
		present.swapchainCount = 1;
		present.pWaitSemaphores = &m_present_ready_ss[image_index];
		present.waitSemaphoreCount = 1;

		assert(vkQueuePresentKHR(m_base.queues[QUEUE_PRESENT], &present) == VK_SUCCESS);
	}
}