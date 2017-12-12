#pragma once

#include "vulkan.h"

#include "freelist_host_memory.h"
#include "vk_allocator.h"

#include "base_create_info.h"
#include "base_info.h"

#include "const_swap_chain_image_count.h"

#include <iostream>

namespace rcq
{

	class base
	{
	public:
		static void init(const base_create_info& info);
		static void destroy();
		static base* instance() { return m_instance; }

		const base_info& get_info() { return m_base_info; };

	private:
		//ctor, dtor, singleton pattern
		base(const base_create_info& info);
		~base();
		base(const base&) = delete;
		base(base&&) = delete;
		base& operator=(const base&) = delete;
		base& operator=(base&&) = delete;
		static base* m_instance;
		
		//info for creation
		const base_create_info m_info;

		//memory resources
		freelist_host_memory m_host_memory;
		vk_allocator m_vk_alloc;

		//info for other classes
		base_info m_base_info;

		//instance, device
		VkInstance m_vk_instance;
		VkPhysicalDevice m_physical_device;
		VkDevice m_device;

		//window, surface
		GLFWwindow* m_window;
		VkSurfaceKHR m_surface;

		//swapchain
		VkSwapchainKHR m_swapchain;
		VkImage m_swapchain_images[SWAP_CHAIN_IMAGE_COUNT];
		VkImageView m_swapchain_views[SWAP_CHAIN_IMAGE_COUNT];

		//queues
		uint32_t m_queue_family_index;
		VkQueue m_queues[QUEUE_COUNT];


		//create functions
		void create_window();
		void create_instance();
		void setup_debug_callbacks();
		void create_surface();
		void pick_physical_device();
		uint32_t find_queue_family_index();
		void create_logical_device();
		void create_swapchain();
		void create_swapchain_views();
		void fill_base_info();



		/*



		bool check_valiadation_layer_support();
		
		bool device_is_suitable(VkPhysicalDevice device);
		bool check_device_extension_support(VkPhysicalDevice device);
		uint32_t find_queue_family_index(VkPhysicalDevice device);*/
		//swap_chain_support_details query_swap_chain_support_details(VkPhysicalDevice device);
		

		//VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& formats);
		//VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& present_modes);
		//VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);


		//debug report
		VkDebugReportCallbackEXT m_callback;

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
	};

}

