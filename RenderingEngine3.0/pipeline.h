#pragma once

#include "vulkan.h"

namespace rcq
{
	struct pipeline
	{
		VkPipeline ppl;
		VkPipelineLayout pl;
		VkDescriptorSet ds;
		VkDescriptorSetLayout dsl;

		void bind(VkCommandBuffer cb, VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS)
		{
			vkCmdBindPipeline(cb, bind_point, ppl);
			vkCmdBindDescriptorSets(cb, bind_point, pl, 0, 1, &ds, 0, nullptr);
		}
	};
}