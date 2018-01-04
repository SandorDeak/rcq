#pragma once

#include "vulkan.h"
#include "glm.h"

#include "enum_rend_type.h"

namespace rcq
{
	template<uint32_t rend_type>
	struct renderable;

	template<>
	struct renderable<REND_TYPE_OPAQUE_OBJECT>
	{
		VkDescriptorSet tr_ds;
		VkDescriptorSet mat_opaque_ds;
		uint32_t mesh_index_size;
		VkBuffer mesh_vb;
		VkBuffer mesh_ib;
		VkBuffer mesh_veb;
	};

	template<>
	struct renderable<REND_TYPE_WATER>
	{
		VkDescriptorSet ds;
		VkDescriptorSet fft_ds;
		glm::vec2 grid_size_in_meters;
	};

	template<>
	struct renderable<REND_TYPE_SKY>
	{
		VkDescriptorSet ds;
	};

	template<>
	struct renderable<REND_TYPE_TERRAIN>
	{
		VkDescriptorSet ds;
		VkDescriptorSet request_ds;
		VkDescriptorSet opaque_material_dss[4];
	};
}