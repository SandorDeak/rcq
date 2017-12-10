#include "gta5_pass.h"

#include "enum_sampler_type.h"

using namespace rcq;

void gta5_pass::create_samplers()
{
	//normalized coord
	{
		VkSamplerCreateInfo s = {};
		s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		s.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.anisotropyEnable = VK_TRUE;
		s.compareEnable = VK_FALSE;
		s.unnormalizedCoordinates = VK_FALSE;
		s.magFilter = VK_FILTER_LINEAR;
		s.minFilter = VK_FILTER_LINEAR;
		s.maxAnisotropy = 16.f;
		s.minLod = 0.f;
		s.maxLod = 0.f;
		s.mipLodBias = 0.f;
		s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		assert(vkCreateSampler(m_base.device, &s, m_vk_alloc, &m_samplers[SAMPLER_TYPE_NORMALIZED_COORD]) == VK_SUCCESS);
	}

	//unnormalized coord
	{
		VkSamplerCreateInfo s = {};
		s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		s.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		s.anisotropyEnable = VK_FALSE;
		s.compareEnable = VK_FALSE;
		s.unnormalizedCoordinates = VK_TRUE;
		s.magFilter = VK_FILTER_NEAREST;
		s.minFilter = VK_FILTER_NEAREST;
		s.minLod = 0.f;
		s.maxLod = 0.f;
		s.mipLodBias = 0.f;
		s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		assert(vkCreateSampler(m_base.device, &s, m_vk_alloc, &m_samplers[SAMPLER_TYPE_UNNORMALIZED_COORD]) == VK_SUCCESS);
	}
}

//DO ALLOCATE AND RECORD COMMAND BUFFERS!!!!!!!!!!!!!