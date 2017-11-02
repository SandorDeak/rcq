#include "base.h"

using namespace rcq;

base* base::m_instance = nullptr;


base::base(const base_create_info& info) : m_info(info), m_alloc("base")
{
	create_window();
	create_instance();

	if (info.enable_validation_layers)
	{
		setup_debug_callbacks();
	}

	create_surface();
	pick_physical_device();
	create_logical_device();
	create_swap_chain();
	create_swap_cain_image_views();
}


base::~base()
{
	for (size_t i = 0; i < m_swap_chain_image_views.size(); ++i)
		vkDestroyImageView(m_device, m_swap_chain_image_views[i], m_alloc);
	vkDestroySwapchainKHR(m_device, m_swap_chain, m_alloc);
	vkDestroyDevice(m_device, m_alloc);
	vkDestroySurfaceKHR(m_vk_instance, m_surface, m_alloc);
	if (m_info.enable_validation_layers)
		DestroyDebugReportCallbackEXT(m_vk_instance, m_callback, m_alloc);
	vkDestroyInstance(m_vk_instance, m_alloc);

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void base::init(const base_create_info& info)
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("base is already initialised!");
	}
	m_instance = new base(info);
}

void base::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy base, it doesn't exist!");
	}

	delete m_instance;
}

void base::create_window()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_window = glfwCreateWindow(m_info.width, m_info.height, m_info.window_name, nullptr, nullptr);

}

bool base::check_valiadation_layer_support()
{
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
	std::vector<VkLayerProperties> available_layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

	for (const char* layer_name : m_info.validation_layers)
	{
		bool found = false;
		for (const auto& layer_properties : available_layers)
		{
			if (strcmp(layer_name, layer_properties.layerName) == 0)
			{
				found = true;
				break;
			}
		}

		if (!found)
			return false;
	}

	return true;
}

void base::create_instance()
{
	if (m_info.enable_validation_layers && !check_valiadation_layer_support())
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Rendering Engine";
	app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "No name yet";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;


	uint32_t glfw_extension_count = 0;
	const char** glfw_extensions;
	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
	std::vector<const char*> required_extensions(glfw_extension_count + m_info.instance_extensions.size());

	for (uint32_t i = 0; i < glfw_extension_count; ++i)
		required_extensions[i] = glfw_extensions[i];
	for (uint32_t i = 0; i < m_info.instance_extensions.size(); ++i)
		required_extensions[i + glfw_extension_count] = m_info.instance_extensions[i];


	create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
	create_info.ppEnabledExtensionNames = required_extensions.data();

	if (m_info.enable_validation_layers)
	{
		create_info.enabledLayerCount = static_cast<uint32_t>(m_info.validation_layers.size());
		create_info.ppEnabledLayerNames = m_info.validation_layers.data();
	}

	else
	{
		create_info.enabledLayerCount = 0;
		create_info.ppEnabledLayerNames = nullptr;
	}

	if (vkCreateInstance(&create_info, m_alloc, &m_vk_instance) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create instance!");
	}

}

void base::setup_debug_callbacks()
{
	VkDebugReportCallbackCreateInfoEXT create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	create_info.pfnCallback = debug_callback;

	if (CreateDebugReportCallbackEXT(m_vk_instance, &create_info, m_alloc, &m_callback) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug callback!");
	}
}

VkResult base::CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback)
{
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}

	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void base::DestroyDebugReportCallbackEXT(VkInstance instance,
	VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");

	if (func != nullptr)
	{
		func(instance, callback, pAllocator);
	}
}

void base::create_surface()
{
	if (glfwCreateWindowSurface(m_vk_instance, m_window, m_alloc, &m_surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create surface!");
	}
}

void base::pick_physical_device()
{
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(m_vk_instance, &device_count, nullptr);
	if (device_count == 0)
	{
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(m_vk_instance, &device_count, devices.data());

	for (const auto& device : devices)
	{
		if (device_is_suitable(device))
		{
			m_physical_device = device;
			m_queue_family_indices = find_queue_family_indices(device);
			return;
		}
	}

	throw std::runtime_error("failed to find suitable GPU!");
}

bool base::device_is_suitable(VkPhysicalDevice device)
{
	if (!check_device_extension_support(device) || !find_queue_family_indices(device).complete())
		return false;


	swap_chain_support_details swap_chain_support = query_swap_chain_support_details(device);
	bool swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();

	if (!swap_chain_adequate)
		return false;

	VkPhysicalDeviceFeatures device_features;
	vkGetPhysicalDeviceFeatures(device, &device_features);


	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(device, &device_properties);
	//std::cout << device_properties.limits.maxFragmentOutputAttachments << std::endl;


	//should write an algorithm for feature compare

	return device_features.samplerAnisotropy;
}

swap_chain_support_details base::query_swap_chain_support_details(VkPhysicalDevice device)
{
	swap_chain_support_details details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);
	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);
	if (format_count != 0)
	{
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.formats.data());
	}

	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);
	if (present_mode_count != 0)
	{
		details.present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.present_modes.data());
	}

	return details;
}


queue_family_indices base::find_queue_family_indices(VkPhysicalDevice device)
{
	queue_family_indices indices;
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

	int i = 0;
	for (const auto& family : queue_families)
	{
		if (family.queueCount == 0)
		{
			++i;
			continue;
		}

		if (family.queueFlags | VK_QUEUE_GRAPHICS_BIT)
			indices.graphics_family = i;

		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

		if (present_support)
			indices.present_family = i;
		if (indices.complete())
			break;

		++i;
	}
	return indices;
}



bool base::check_device_extension_support(VkPhysicalDevice device)
{
	uint32_t extension_count;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

	for (const char* required_extension : m_info.device_extensions)
	{
		bool found = false;
		for (const auto& available_extension : available_extensions)
		{
			if (strcmp(required_extension, available_extension.extensionName) == 0)
			{
				found = true;
				break;
			}
		}

		if (!found)
			return false;
	}

	return true;
}

void base::create_logical_device()
{
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::set<int> unique_queue_families = { m_queue_family_indices.graphics_family, m_queue_family_indices.present_family };

	float queue_priority = 1.0f;

	for (int queue_family : unique_queue_families)
	{
		VkDeviceQueueCreateInfo queue_create_info = {};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	create_info.pEnabledFeatures = &m_info.device_features;
	create_info.ppEnabledExtensionNames = m_info.device_extensions.data();
	create_info.enabledExtensionCount = static_cast<uint32_t>(m_info.device_extensions.size());

	if (m_info.enable_validation_layers)
	{
		create_info.enabledLayerCount = static_cast<uint32_t>(m_info.validation_layers.size());
		create_info.ppEnabledLayerNames = m_info.validation_layers.data();
	}
	else
	{
		create_info.enabledLayerCount = 0;
	}

	if (vkCreateDevice(m_physical_device, &create_info, m_alloc, &m_device) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical device!");
	}

	vkGetDeviceQueue(m_device, m_queue_family_indices.graphics_family, 0, &m_graphics_queue);
	vkGetDeviceQueue(m_device, m_queue_family_indices.present_family, 0, &m_present_queue);
}

void base::create_swap_chain()
{
	swap_chain_support_details swap_chain_support = query_swap_chain_support_details(m_physical_device);

	VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support.formats);
	VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.present_modes);
	VkExtent2D extent = choose_swap_extent(swap_chain_support.capabilities);

	uint32_t image_count;
	if (swap_chain_support.capabilities.maxImageCount == 0) //means no max limit
		image_count = swap_chain_support.capabilities.minImageCount;
	else
		image_count = std::min(swap_chain_support.capabilities.minImageCount, swap_chain_support.capabilities.maxImageCount);

	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = m_surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.presentMode = present_mode;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t family_indices[] = { static_cast<uint32_t>(m_queue_family_indices.graphics_family),
		static_cast<uint32_t>(m_queue_family_indices.present_family) };

	if (m_queue_family_indices.graphics_family == m_queue_family_indices.present_family)
	{
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;
	}
	else
	{
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = family_indices;
	}

	create_info.preTransform = swap_chain_support.capabilities.currentTransform;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_device, &create_info, m_alloc, &m_swap_chain) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, nullptr);
	m_swap_chain_images.resize(image_count);
	vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, m_swap_chain_images.data());

	m_swap_chin_image_format = surface_format.format;
	m_swap_chain_extent = extent;
}

VkSurfaceFormatKHR base::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& formats)
{
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

	for (const auto& available_format : formats)
	{
		if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return available_format;
	}
	return formats[0];
}
VkPresentModeKHR base::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& present_modes)
{
	VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto present_mode : present_modes)
	{
		if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
			return present_mode;

		else if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			best_mode = present_mode;
	}

	return best_mode;
}
VkExtent2D base::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		return capabilities.currentExtent;

	int width, height;
	glfwGetWindowSize(m_window, &width, &height);

	VkExtent2D actual_extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(actual_extent.width, capabilities.maxImageExtent.width));
	actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(actual_extent.height, capabilities.maxImageExtent.height));

	return actual_extent;
}

void base::create_swap_cain_image_views()
{
	m_swap_chain_image_views.resize(m_swap_chain_images.size());
	for (unsigned int i = 0; i < m_swap_chain_image_views.size(); ++i)
	{
		VkImageViewCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.format = m_swap_chin_image_format;
		create_info.image = m_swap_chain_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.layerCount = 1;
		create_info.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_device, &create_info, m_alloc, &m_swap_chain_image_views[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image view!");
		}
	}
}

base_info base::get_info()
{
	base_info info = {};
	info.device = m_device;
	info.graphics_queue = m_graphics_queue;
	info.physical_device = m_physical_device;
	info.present_queue = m_present_queue;
	info.queue_families = m_queue_family_indices;
	info.swap_chain = m_swap_chain;
	info.swap_chain_image_extent = m_swap_chain_extent;
	info.swap_chain_image_format = m_swap_chin_image_format;
	info.swap_chain_image_views = m_swap_chain_image_views;
	info.window = m_window;

	return info;
}