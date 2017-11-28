#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_SWIZZLE
#include<glm\glm.hpp>
#include <glm\gtx\hash.hpp>
#include <glm\gtx\transform.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>

#include "slot_map.h"

#include <atomic>
#include <array>


namespace rcq
{
	enum RES_TYPE
	{
		RES_TYPE_MAT_OPAQUE,
		RES_TYPE_MAT_EM,
		RES_TYPE_SKY,
		RES_TYPE_TERRAIN,
		RES_TYPE_WATER,
		//RES_TYPE_LIGHT_OMNI,
		//RES_TYPE_SKYBOX,
		RES_TYPE_MESH,
		RES_TYPE_TR,
		RES_TYPE_MEMORY,
		RES_TYPE_COUNT
	};

	enum TEX_TYPE
	{
		TEX_TYPE_COLOR,
		TEX_TYPE_ROUGHNESS,
		TEX_TYPE_METAL,
		TEX_TYPE_NORMAL,
		TEX_TYPE_HEIGHT,
		TEX_TYPE_AO,
		TEX_TYPE_COUNT
	};




	struct base_resource
	{
		std::atomic_bool ready;
		VkFence ready_f;
	};

	template<size_t res_type>
	struct resource;

	template<>
	struct resource<RES_TYPE_MAT_OPAQUE> : public base_resource
	{
		struct build_info
		{
			char texfiles[TEX_TYPE_COUNT][128];
			glm::vec3 color;
			float roughness;
			float meta;
			float height_scale;
			uint32_t tex_flags;
		};

		typedef slot_map<resource<RES_TYPE_MAT_OPAQUE>, dealloc_pattern::ignore> container_type;

		struct texture
		{
			VkImage image;
			VkImageView view;
			VkDeviceMemory memory;
		};

		std::array<texture, TEX_TYPE_COUNT> texs;
		VkDescriptorSet ds;
		uint32_t pool_index;
		VkBuffer data;
		//cell_info cell;
	};

	template<>
	struct resource<RES_TYPE_MESH>
	{
		struct build_info
		{
			char filename[128];
			bool calc_tb;
		};

		typedef slot_map<resource<RES_TYPE_MESH>, dealloc_pattern::ignore> container_tpye;

		VkBuffer vb; //vertex
		VkBuffer ib; //index
		VkBuffer veb; //vertex ext
		VkDeviceMemory memory;
		uint32_t size;
	};

	template<>
	struct resource<RES_TYPE_TR>
	{
		struct build_info
		{
			glm::mat4 model;
			glm::vec3 scale;
			glm::vec2 tex_scale;
		};

		typedef slot_map<resource<RES_TYPE_TR>, dealloc_pattern::ignore> container_tpye;

		VkDescriptorSet ds;
		uint32_t pool_index;
		USAGE usage;
		VkBuffer buffer;
		//cell_info cell;
	};



	const uint32_t MAX_BUILD_INFO_SIZE = 1;
	struct res_build_info
	{
		uint64_t id;
		RES_TYPE res_type;
		char data[MAX_BUILD_INFO_SIZE];
	};

	template<size_t... res_types>
	class resource_container 
	{
	public:
		template<size_t res_type>
		resource<res_type>* get(uint64_t id)
		{
			return std::get<res_type>(m_data).get(id);
		}

		template<res_type>
		resource<res_type>* get_new()
		{
			return std::get<res_type>(m_data).emplace();
		}

	private:
		std::tuple<resource<res_types>::container_type...> m_data;
	};

	template<typename T>
	class static_queue
	{
	public:
		static_queue(uint32_t size) :
			m_physical_size(size),
			m_size(0),
			m_front(0)
		{
			m_data = reinterpret_cast<T*>(std::malloc(size * sizeof(T)));
		}

		~static_queue()
		{
			std::free(m_data);
		}

		uint32_t size() { return m_size; }

		T* front()
		{
			return m_data+m_front;
		}

		void pop()
		{
			m_front = (m_front + 1) % m_physical_size;
			m_size=(m_size+m_physical_size-1)%m_physical_size;
		}

		T* next_available()
		{
			if (m_size == m_physical_size)
				return nullptr;

			return m_data[m_back];
		}

		void push()
		{
			m_size=(m_size+1)%m_physical_size;
			m_back = (m_back + 1) % m_physical_size;
		}

		bool empty() { return m_size == 0; }

		

	private:
		T* m_data;
		uint32_t m_front;
		uint32_t m_back;
		std::atomic<uint32_t> m_size;
		uint32_t m_physical_size;
	};


} //namespace rcq