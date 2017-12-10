#pragma once

#include "vulkan.h"
#include "glm.h"
#include <array>

namespace rcq
{

	struct vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 tex_coord;

		constexpr static VkVertexInputBindingDescription get_binding_description()
		{
			VkVertexInputBindingDescription binding_description = {};
			binding_description.binding = 0;
			binding_description.stride = sizeof(vertex);
			binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return binding_description;
		}

		constexpr static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions()
		{
			std::array<VkVertexInputAttributeDescription, 3> attribute_description = {};
			attribute_description[0].binding = 0;
			attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_description[0].location = 0;
			attribute_description[0].offset = offsetof(vertex, pos);

			attribute_description[1].binding = 0;
			attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_description[1].location = 1;
			attribute_description[1].offset = offsetof(vertex, normal);

			attribute_description[2].binding = 0;
			attribute_description[2].format = VK_FORMAT_R32G32_SFLOAT;
			attribute_description[2].location = 2;
			attribute_description[2].offset = offsetof(vertex, tex_coord);

			return attribute_description;
		}


		bool operator==(const vertex& other) const
		{
			return pos == other.pos && normal == other.normal && tex_coord == other.tex_coord;
		}
	};

	struct vertex_ext
	{
		glm::vec3 tangent;
		glm::vec3 bitangent;

		constexpr static VkVertexInputBindingDescription get_binding_description()
		{
			VkVertexInputBindingDescription binding_description = {};
			binding_description.binding = 1;
			binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			binding_description.stride = sizeof(vertex_ext);
			return binding_description;
		}

		constexpr static std::array<VkVertexInputAttributeDescription, 2> get_attribute_descriptions()
		{
			std::array<VkVertexInputAttributeDescription, 2> attribute_description = {};
			attribute_description[0].binding = 1;
			attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_description[0].location = 3;
			attribute_description[0].offset = offsetof(vertex_ext, tangent);

			attribute_description[1].binding = 1;
			attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_description[1].location = 4;
			attribute_description[1].offset = offsetof(vertex_ext, bitangent);

			return attribute_description;
		}
	};

	constexpr decltype(auto) get_vertex_input_binding_descriptions()
	{
		auto vertex_binding = vertex::get_binding_description();
		auto vertex_ext_binding = vertex_ext::get_binding_description();
		return std::array<VkVertexInputBindingDescription, 2>{vertex_binding, vertex_ext_binding};
	}

	constexpr decltype(auto) get_vertex_input_attribute_descriptions()
	{
		auto vertex_attribs = vertex::get_attribute_descriptions();
		auto vertex_ext_attribs = vertex_ext::get_attribute_descriptions();
		std::array<VkVertexInputAttributeDescription, vertex_attribs.size() + vertex_ext_attribs.size()> attribs = {};
		for (size_t i = 0; i < vertex_attribs.size(); ++i)
			attribs[i] = vertex_attribs[i];
		for (size_t i = 0; i < vertex_ext_attribs.size(); ++i)
			attribs[i + vertex_attribs.size()] = vertex_ext_attribs[i];
		return attribs;
	}
}