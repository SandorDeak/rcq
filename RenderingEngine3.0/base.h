#pragma once

#include "foundation.h"
#include <iostream>

namespace rcq
{

	class base
	{
	public:
		~base();
		base(const base&) = delete;
		base(base&&) = delete;

		static void init(const base_create_info& info);
		static void destroy();
		static base* instance() { return m_instance; }

		base_info get_info();
	private:
		base(const base_create_info& info);

		static base* m_instance;

		const base_create_info m_info;

		void create_window();
		bool check_valiadation_layer_support();
		void create_instance();
		void setup_debug_callbacks();
		void create_surface();
		void pick_physical_device();
		bool device_is_suitable(VkPhysicalDevice device);
		bool check_device_extension_support(VkPhysicalDevice device);
		queue_family_indices find_queue_family_indices(VkPhysicalDevice device);
		swap_chain_support_details query_swap_chain_support_details(VkPhysicalDevice device);
		void create_logical_device();
		void create_swap_chain();
		VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& formats);
		VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& present_modes);
		VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
		void create_swap_cain_image_views();

		static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
			VkDebugReportFlagsEXT flags,
			VkDebugReportObjectTypeEXT obj_type,
			uint64_t obj,
			size_t location,
			int32_t code,
			const char* layer_prefix,
			const char* msg,
			void* user_data)
		{
			std::cerr << "validation layer: " << msg << std::endl;
			return VK_FALSE;
		}

		static VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
			const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback);
		static void DestroyDebugReportCallbackEXT(VkInstance instance,
			VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);


		VkInstance m_vk_instance;
		GLFWwindow* m_window;
		VkPhysicalDevice m_physical_device;
		VkDevice m_device;
		VkDebugReportCallbackEXT m_callback;

		VkSurfaceKHR m_surface;

		queue_family_indices m_queue_family_indices;
		VkQueue m_graphics_queue;
		VkQueue m_compute_queue;
		VkQueue m_present_queue;

		VkSwapchainKHR m_swap_chain;
		std::vector<VkImage> m_swap_chain_images;
		std::vector<VkImageView> m_swap_chain_image_views;
		VkFormat m_swap_chin_image_format;
		VkExtent2D m_swap_chain_extent;

		//allocator
		allocator m_alloc;
	};

}

