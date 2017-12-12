#include "engine.h"

#include "enum_gp.h"
#include "enum_rp.h"
#include "enum_cp.h"
#include "enum_res_image.h"

#include "const_swap_chain_image_count.h"
#include "const_swap_chain_image_extent.h"

using namespace rcq;

void engine::allocate_and_record_cbs()
{
	/////////////////////////////////////////////////////////
	//create command pools

	VkCommandPoolCreateInfo pool = {};
	pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool.queueFamilyIndex = m_base.queue_family_index;
	assert(vkCreateCommandPool(m_base.device, &pool, m_vk_alloc, &m_cpools[CPOOL_GRAPHICS]) == VK_SUCCESS);
	assert(vkCreateCommandPool(m_base.device, &pool, m_vk_alloc, &m_cpools[CPOOL_PRESENT]) == VK_SUCCESS);
	assert(vkCreateCommandPool(m_base.device, &pool, m_vk_alloc, &m_cpools[CPOOL_COMPUTE]) == VK_SUCCESS);


	/////////////////////////////////////////////////////////
	//allocate

	//render cb
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = 1;
		alloc.commandPool = m_cpools[CPOOL_GRAPHICS];
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_cbs[CB_RENDER]) == VK_SUCCESS);

	}

	//present cbs
	{

		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = SWAP_CHAIN_IMAGE_COUNT;
		alloc.commandPool = m_cpools[CPOOL_GRAPHICS];
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, m_present_cbs) == VK_SUCCESS);
	}

	//compute cbs
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = 1;
		alloc.commandPool = m_cpools[CPOOL_COMPUTE];
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_cbs[CB_TERRAIN_REQUEST]) == VK_SUCCESS);
		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_cbs[CB_WATER_FFT]) == VK_SUCCESS);
		assert(vkAllocateCommandBuffers(m_base.device, &alloc, &m_cbs[CB_BLOOM]) == VK_SUCCESS);
	}

	//secondary cbs
	{
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandBufferCount = SECONDARY_CB_COUNT;
		alloc.commandPool = m_cpools[CPOOL_GRAPHICS];
		alloc.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

		assert(vkAllocateCommandBuffers(m_base.device, &alloc, m_secondary_cbs) == VK_SUCCESS);
	}

	/////////////////////////////////////////////////////////
	//record

	//present cbs
	for (uint32_t i = 0; i < SWAP_CHAIN_IMAGE_COUNT; ++i)
	{
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		assert(vkBeginCommandBuffer(m_present_cbs[i], &begin) == VK_SUCCESS);

		VkRenderPassBeginInfo pass = {};
		pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		pass.clearValueCount = 0;
		pass.framebuffer = m_postprocessing_fbs[i];
		pass.renderPass = m_rps[RP_POSTPROCESSING];
		pass.renderArea.extent = SWAP_CHAIN_IMAGE_EXTENT;
		pass.renderArea.offset = { 0,0 };

		vkCmdBeginRenderPass(m_present_cbs[i], &pass, VK_SUBPASS_CONTENTS_INLINE);
		m_gps[GP_POSTPROCESSING].bind(m_present_cbs[i]);
		vkCmdDraw(m_present_cbs[i], 4, 1, 0, 0);
		vkCmdEndRenderPass(m_present_cbs[i]);

		assert(vkEndCommandBuffer(m_present_cbs[i]) == VK_SUCCESS);
	}

	//bloom blur cb
	{
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		assert(vkBeginCommandBuffer(m_cbs[CB_BLOOM], &begin) == VK_SUCCESS);

		glm::ivec2 size = { SWAP_CHAIN_IMAGE_EXTENT.width / BLOOM_IMAGE_SIZE_FACTOR,
			SWAP_CHAIN_IMAGE_EXTENT.height / BLOOM_IMAGE_SIZE_FACTOR };

		m_cps[CP_BLOOM].bind(m_cbs[CB_BLOOM], VK_PIPELINE_BIND_POINT_COMPUTE);
		uint32_t step = 0;
		vkCmdPushConstants(m_cbs[CB_BLOOM], m_cps[CP_BLOOM].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &step);
		vkCmdDispatch(m_cbs[CB_BLOOM], size.x, size.y, 1);

		for (uint32_t i = 0; i < 1; ++i)
		{
			VkMemoryBarrier b = {};
			b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(m_cbs[CB_BLOOM], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 1, &b, 0, nullptr, 0, nullptr);

			step = 1;
			vkCmdPushConstants(m_cbs[CB_BLOOM], m_cps[CP_BLOOM].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &step);
			vkCmdDispatch(m_cbs[CB_BLOOM], size.x, size.y, 1);

			vkCmdPipelineBarrier(m_cbs[CB_BLOOM], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 1, &b, 0, nullptr, 0, nullptr);

			step = 2;
			vkCmdPushConstants(m_cbs[CB_BLOOM], m_cps[CP_BLOOM].pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &step);
			vkCmdDispatch(m_cbs[CB_BLOOM], size.x, size.y, 1);
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

			vkCmdPipelineBarrier(m_cbs[CB_BLOOM], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &b);
		}
		assert(vkEndCommandBuffer(m_cbs[CB_BLOOM]) == VK_SUCCESS);
	}
}