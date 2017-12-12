#include "engine.h"

#include "resources.h"
#include "enum_cp.h"

using namespace rcq;

void engine::set_water(base_resource* water)
{
	auto w = reinterpret_cast<resource<RES_TYPE_WATER>*>(water->data);
	m_water.ds = w->ds;
	m_water.fft_ds = w->fft_ds;
	m_water.grid_size_in_meters = w->grid_size_in_meters;

	//record water fft cb
	auto cb = m_cbs[CB_WATER_FFT];

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