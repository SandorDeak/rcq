#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_SWIZZLE
#include<glm\glm.hpp>
#include <glm\gtx\hash.hpp>
#include <glm\gtx\transform.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>

#include <string>
#include <vector>
#include <stack>
#include <map>
#include <stdexcept>
#include <chrono>
#include <array>
#include <set>



namespace rcq
{
	//interface related

	enum TEX_TYPE
	{
		TEX_TYPE_COLOR,
		TEX_TYPE_ROUGHNESS,
		TEX_TYPE_METAL,
		TEX_TYPE_NORMAL,
		TEY_TYPE_HEIGHT,
		TEX_TYPE_AO,
		TEX_TYPE_COUNT
	};

	enum USAGE
	{
		USAGE_STATIC,
		USAGE_DYNAMIC
	};

	enum LIFE_EXPECTANCY
	{
		LIFE_EXPECTANCY_LONG,
		LIFE_EXPECTANCY_SHORT
	};

	struct material_data;
	struct transform_data;
	struct camera_data;

	typedef std::array<std::string, TEX_TYPE_COUNT> texinfos;
	typedef std::string filename;
	typedef size_t unique_id;

	typedef std::tuple<unique_id, const material_data*, texinfos, USAGE> build_mat_info;
	enum 
	{
		BUILD_MAT_INFO_MAT_ID,
		BUILD_MAT_INFO_MAT_DATA_POINTER,
		BUILD_MAT_INFO_TEXINFOS,
		BUILD_MAT_INFO_USAGE
	};

	typedef std::tuple<unique_id, filename, bool> build_mesh_info;
	enum
	{
		BUILD_MESH_INFO_MESH_ID,
		BUILD_MESH_INFO_FILENAME,
		BUILD_MESH_INFO_CALC_TB  //calculate tangent and bitangent vectors

	};

	typedef std::tuple<unique_id, const transform_data*, unique_id, unique_id, LIFE_EXPECTANCY> build_renderable_info;
	enum
	{
		BUILD_RENDERABLE_INFO_RENDERABLE_ID,
		BUILD_RENDERABLE_INFO_TRANSFORM_DATA_POINTER,
		BUILD_RENDERABLE_INFO_MESH_ID,
		BUILD_RENDERABLE_INFO_MAT_ID,
		BUILD_RENDERABLE_INFO_LIFE_EXPECTANCY
	};


	struct material_data
	{
		uint32_t flags;
		float metal;
		float roughness;
		float refraction_index;
	};


	struct transform_data
	{
		glm::mat4 model;
		glm::vec3 scale;
	private:
		uint32_t padding0;
	public:
		glm::vec2 tex_scale;
	private:
		uint32_t padding1[2];
	};

	struct camera_data
	{
		glm::mat4 proj_x_view;
		glm::vec3 pos;
	private:
		uint32_t padding0;
	};


	//engine related
	extern const VkAllocationCallbacks* host_memory_manager;

	struct base_create_info
	{
		std::vector<const char*> validation_layers;
		std::vector<const char*> instance_extensions;
		std::vector<const char*> device_extensions;
		int width;
		int height;
		const char* window_name;
		bool enable_validation_layers;
		VkPhysicalDeviceFeatures device_features;
	};

	struct queue_family_indices
	{
		int graphics_family = -1;
		int present_family = -1;

		bool complete()
		{
			return graphics_family >= 0 && present_family >= 0;
		}
	};

	struct swap_chain_support_details
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	struct base_info
	{
		VkDevice device;
		std::vector<VkImageView> swap_chain_image_views;
		VkSwapchainKHR swap_chain;
		VkFormat swap_chain_image_format;
		VkExtent2D swap_chain_image_extent;
		VkPhysicalDevice physical_device;
		queue_family_indices queue_families;
		VkQueue graphics_queue;
		VkQueue present_queue;
		GLFWwindow* window;
	};
}
