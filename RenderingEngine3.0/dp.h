#pragma once

#include "cps.h"
#include "gps.h"

namespace rcq
{
	template<VkDescriptorType type, typename ArrayType>
	constexpr void count_descriptor_type_in_bindings(ArrayType&& bindings, uint32_t& count)
	{
		for (auto& b : bindings)
		{
			if (type == b.descriptorType)
				++count;
		}
	}

	template<VkDescriptorType type, uint32_t... gp_ids>
	constexpr uint32_t count_descriptor_type_gp(std::index_sequence<gp_ids...>)
	{
		uint32_t count = 0;
		auto l = { (count_descriptor_type_in_bindings<type>(gp_create_info<gp_ids>::dsl::bindings, count), 0)... };
		return count;
	}

	template<VkDescriptorType type, uint32_t... cp_ids>
	constexpr uint32_t count_descriptor_type_cp(std::index_sequence<cp_ids...>)
	{
		uint32_t count = 0;
		auto l = { (count_descriptor_type_in_bindings<type>(cp_create_info<cp_ids>::dsl::bindings, count), 0)... };
		return count;
	}

	template<VkDescriptorType type>
	constexpr uint32_t count_descriptor_type()
	{
		return
			count_descriptor_type_gp<type>(std::make_index_sequence<GP_COUNT>())
			+ count_descriptor_type_cp<type>(std::make_index_sequence<CP_COUNT>());
	}

	struct dp
	{
		static constexpr std::array<VkDescriptorPoolSize, 4> sizes =
		{
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			count_descriptor_type<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>(),

			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			count_descriptor_type<VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER>(),

			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			count_descriptor_type<VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT>(),

			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			count_descriptor_type<VK_DESCRIPTOR_TYPE_STORAGE_IMAGE>()
		};
		static constexpr VkDescriptorPoolCreateInfo create_info =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			nullptr,
			0,
			GP_COUNT + CP_COUNT,
			sizes.size(),
			sizes.data(),
		};
	};
}