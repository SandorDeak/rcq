#include "utility.h"

#include <assert.h>
#include <fstream>
#include "vector.h"
#include "os_memory.h"

using namespace rcq;

/*uint32_t utility::find_memory_type(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
	{
		if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties))
			return i;
	}

	assert(false);
	return ~0;
}*/

void utility::read_file(const char* filename, char* dst, uint32_t& size)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	assert(file.is_open());

	size = (uint32_t)file.tellg();

	file.seekg(0);
	file.read(dst, size);
	file.close();
}

void utility::create_shaders(VkDevice device, const char** files, const VkShaderStageFlagBits* stages,
	VkPipelineShaderStageCreateInfo* shaders, uint32_t shader_count)
{
	for (uint32_t i = 0; i < shader_count; ++i)
	{
		std::ifstream file(files[i], std::ios::ate | std::ios::binary);
		assert(file.is_open());

		size_t size = static_cast<size_t>(file.tellg());
		vector<char> code(&OS_MEMORY, size);

		file.seekg(0);
		file.read(code.data(), size);
		file.close();
		VkShaderModuleCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = size; 
		info.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shader_module;
		assert(vkCreateShaderModule(device, &info, nullptr, &shader_module) == VK_SUCCESS);

		shaders[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaders[i].module = shader_module;
		shaders[i].pName = "main";
		shaders[i].stage = stages[i];
	}
}

void utility::load_mesh(vector<vertex>& vertices, vector<uint32_t>& indices, vector<vertex_ext>& vertices_ext, bool calc_tb,
	const char* filename, host_memory* memory)
{
	vector<glm::vec3> v(memory);
	vector<glm::vec3> vn(memory);
	vector<glm::vec2> vt(memory);
	vector<uint32_t[3]> vertex_infos(memory);  //vertex id, texcoord id, normal id

	std::ifstream file(filename);
	char line[512];
	float max_r_square = 0.f;

	//read from file, fill indices
	while (file.getline(line, 512))
	{
		if (line[0] == 'v' && line[1] == ' ')
		{
			char* c = strchr(line, ' ');
			glm::vec3& coords = *v.push_back();
			coords.x = strtof(c, &c);
			coords.y = strtof(c, &c);
			coords.z = strtof(c, &c);
			float length_square = glm::dot(coords, coords);
			max_r_square = max_r_square < length_square ? length_square : max_r_square;
		}
		if (line[0] == 'v' && line[1] == 't')
		{
			char* c = strchr(line, ' ');
			glm::vec2& tex_coords = *vt.push_back();
			tex_coords.x = strtof(c, &c);
			tex_coords.y = 1.f - strtof(c, &c);
		}
		if (line[0] == 'v' && line[1] == 'n')
		{
			char* c = strchr(line, ' ');
			glm::vec3& coords = *vn.push_back();
			coords.x = strtof(c, &c);
			coords.y = strtof(c, &c);
			coords.z = strtof(c, &c);
		}

		if (line[0] == 'f' && line[1] == ' ')
		{
			uint32_t face_indices[8];
			uint32_t face_index_count = 0;

			char* c = strchr(line, ' ');
			while (c)
			{
				uint32_t current_vertex_info[3];

				//read v, vt and vn ids
				current_vertex_info[0] = static_cast<uint32_t>(strtoul(c, &c, 10));
				if (*c == '/')
				{
					++c;
					if (*c == '/')
					{
						++c;
						current_vertex_info[1] = ~0;
						current_vertex_info[2]= static_cast<uint32_t>(strtoul(c, &c, 10));
					}
					else
					{
						current_vertex_info[1]= static_cast<uint32_t>(strtoul(c, &c, 10));
						if (*c == '/')
						{
							++c;
							current_vertex_info[2] = static_cast<uint32_t>(strtoul(c, &c, 10));
						}
					}
				}
				//check if current_vertex_info is already contained in vertex infos
				uint32_t index = 0;
				for (auto& vertex_info : vertex_infos)
				{
					if (memcmp(vertex_info, current_vertex_info, sizeof(uint32_t) * 3) == 0)
						break;
					++index;
				}
				if (index == vertex_infos.size())
					memcpy(vertex_infos.push_back(), current_vertex_info, sizeof(uint32_t) * 3);
				face_indices[face_index_count++] = index;

				c = strchr(c, ' ');
				if (c != nullptr && c[1] == '\0')
					break;
			}
			for (uint32_t i = 2; i < face_index_count; ++i)
			{
				*indices.push_back() = face_indices[0];
				*indices.push_back() = face_indices[i - 1];
				*indices.push_back() = face_indices[i];
			}
		}
	}
	file.close();

	//fill vertices
	vertices.resize(vertex_infos.size());
	auto it1 = vertices.begin();
	auto it2 = vertex_infos.begin();

	while (it1 != vertices.end())
	{
		it1->pos = v[(*it2)[0]-1];
		if (!vt.empty())
			it1->tex_coord = vt[(*it2)[1]-1];
		it1->normal = vn[(*it2)[2]-1];

		++it1;
		++it2;
	}

	//calculate tangent and bitangent vectors if required
	if (calc_tb)
	{
		vertices_ext.resize(vertices.size());
		for (auto& ve : vertices_ext)
			new(&ve.tangent) glm::vec3(0.f);

		for (uint32_t i=0; i<indices.size(); i+=3)
		{
			glm::vec2 t0 = vertices[indices[i + 2]].tex_coord;
			glm::vec3 p0 = vertices[indices[i + 2]].pos;
			glm::vec2 t1 = vertices[indices[i + 1]].tex_coord;
			glm::vec3 p1 = vertices[indices[i + 1]].pos;
			glm::vec2 t2 = vertices[indices[i]].tex_coord;
			glm::vec3 p2 = vertices[indices[i]].pos;

			glm::vec3 tangent = glm::mat2x3(p1 - p0, p2 - p0)*glm::inverse(glm::mat2(t1 - t0, t2 - t0))[0];

			for (uint32_t j = i; j < i + 3; ++j)
				vertices_ext[indices[j]].tangent += tangent;
		}

		auto it1 = vertices.begin();
		auto it2 = vertices_ext.begin();

		while (it1 != vertices.end())
		{
			it2->tangent -= glm::dot(it2->tangent, it1->normal)*it1->normal;
			assert(it2->tangent != glm::vec3(0.f));
			it2->tangent = glm::normalize(it2->tangent);
			it2->bitangent = glm::cross(it2->tangent, it1->normal);

			++it1;
			++it2;
		}
	}


}
