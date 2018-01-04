#pragma once

#include "cps.h"
#include "gps.h"

namespace rcq
{
	template<size_t N>
	constexpr uint32_t count_descriptor_type_in_bindings(VkDescriptorType type,
		const std::array<VkDescriptorSetLayoutBinding, N>& bindings)
	{
		uint32_t count = 0;
		for (auto& b : bindings)
		{
			if (type == b.descriptorType)
				++count;
		}
		return count;
	}

	template<uint32_t gp_id, uint32_t... tail>
	constexpr uint32_t count_descriptor_type_gp_impl(VkDescriptorType type)
	{
		if constexpr (sizeof...(tail) == 0)
			return count_descriptor_type_in_bindings(type, gp_create_info<gp_id>::dsl::bindings);
		else
			return count_descriptor_type_in_bindings(type, gp_create_info<gp_id>::dsl::bindings) +
			count_descriptor_type_gp_impl<tail...>(type);
	}

	template<uint32_t cp_id, uint32_t... tail>
	constexpr uint32_t count_descriptor_type_cp_impl(VkDescriptorType type)
	{
		if constexpr (sizeof...(tail) == 0)
			return count_descriptor_type_in_bindings(type, cp_create_info<cp_id>::dsl::bindings);
		else
			return count_descriptor_type_in_bindings(type, cp_create_info<cp_id>::dsl::bindings) +
			count_descriptor_type_cp_impl<tail...>(type);
	}

	template<uint32_t... gp_ids>
	constexpr uint32_t count_descriptor_type_gp(VkDescriptorType type, std::index_sequence<gp_ids...>)
	{
		return count_descriptor_type_gp_impl<gp_ids...>(type);
	}

	template<uint32_t... cp_ids>
	constexpr uint32_t count_descriptor_type_cp(VkDescriptorType type, std::index_sequence<cp_ids...>)
	{
		return count_descriptor_type_cp_impl<cp_ids...>(type);
	}

	constexpr uint32_t count_descriptor_type(VkDescriptorType type)
	{
		return
			count_descriptor_type_gp(type, std::make_index_sequence<GP_COUNT>())
			+ count_descriptor_type_cp(type, std::make_index_sequence<CP_COUNT>());
	}
}

namespace rcq::dp
{
	constexpr uint32_t ub_count = count_descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	constexpr uint32_t cis_count = count_descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	constexpr uint32_t ia_count = count_descriptor_type(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
	constexpr uint32_t si_count = count_descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	constexpr std::array<VkDescriptorPoolSize, 4> sizes =
	{
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		ub_count,

		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		cis_count,

		VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
		ia_count,

		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		si_count
	};
	constexpr VkDescriptorPoolCreateInfo create_info =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		nullptr,
		0,
		GP_COUNT + CP_COUNT,
		sizes.size(),
		sizes.data(),
	};
}