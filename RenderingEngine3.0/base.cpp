#include "base.h"

#include "const_swap_chain_image_extent.h"
#include "const_max_alignment.h"

#include "os_memory.h"

#include <assert.h>

using namespace rcq;

base* base::m_instance = nullptr;


base::base(const base_create_info& info) : 
	m_info(info),
	m_host_memory(64*1024*1024, MAX_ALIGNMENT, &OS_MEMORY)
{
	m_vk_alloc.init(&m_host_memory);

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
	fill_base_info();
}


base::~base()
{
	for (auto& v : m_swapchain_views)
		vkDestroyImageView(m_device, v, m_vk_alloc);
	vkDestroySwapchainKHR(m_device, m_swapchain, m_vk_alloc);
	vkDestroyDevice(m_device, m_vk_alloc);
	vkDestroySurfaceKHR(m_vk_instance, m_surface, m_vk_alloc);
	if (m_info.enable_validation_layers)
		DestroyDebugReportCallbackEXT(m_vk_instance, m_callback, m_vk_alloc);
	vkDestroyInstance(m_vk_instance, m_vk_alloc);

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void base::init(const base_create_info& info)
{
	assert(m_instance == nullptr);
	m_instance = new base(info);
}

void base::destroy()
{
	assert(m_instance != nullptr);
	delete m_instance;
	m_instance = nullptr;
}

void base::create_window()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_window = glfwCreateWindow(SWAP_CHAIN_IMAGE_EXTENT.width, SWAP_CHAIN_IMAGE_EXTENT.height, m_info.window_name, nullptr, nullptr);
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
	assert(!m_info.enable_validation_layers || check_valiadation_layer_support());

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "rcq rendering engine";
	app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "rcq";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;


	uint32_t glfw_extension_count;
	const char** glfw_extensions;
	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
	const char* required_extensions[32];
	uint32_t required_extension_count = glfw_extension_count + m_info.instance_extensions_count;
	assert(required_extension_count <= 32);

	for (uint32_t i = 0; i < glfw_extension_count; ++i)
		required_extensions[i] = glfw_extensions[i];
	for (uint32_t i = 0; i < m_info.instance_extensions_count; ++i)
		required_extensions[i + glfw_extension_count] = m_info.instance_extensions[i];

	create_info.enabledExtensionCount = require;
	create_info.ppEnabledExtensionNames = required_extensions;

	if (m_info.enable_validation_layers)
	{
		create_info.enabledLayerCount = m_info.validation_layer_count;
		create_info.ppEnabledLayerNames = m_info.validation_layers;
	}
	else
	{
		create_info.enabledLayerCount = 0;
		create_info.ppEnabledLayerNames = nullptr;
	}

	assert(vkCreateInstance(&create_info, m_vk_alloc, &m_vk_instance) == VK_SUCCESS);
}

void base::setup_debug_callbacks()
{
	VkDebugReportCallbackCreateInfoEXT create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	create_info.pfnCallback = debug_callback;

	assert(CreateDebugReportCallbackEXT(m_vk_instance, &create_info, m_vk_alloc, &m_callback) == VK_SUCCESS);
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
	assert(glfwCreateWindowSurface(m_vk_instance, m_window, m_vk_alloc, &m_surface) == VK_SUCCESS);
}

void base::pick_physical_device()
{
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(m_vk_instance, &device_count, nullptr);
	assert(device_count != 0);

	vkEnumeratePhysicalDevices(m_vk_instance, 1, &m_physical_device);

	m_queue_family_index = find_queue_family_index(m_physical_device);
}

uint32_t base::find_queue_family_index()
{
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_device, &queue_family_count, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(m_device, &queue_family_count, queue_families.data());

	int i = 0;
	for (const auto& family : queue_families)
	{
		if (family.queueCount < 4)
		{
			++i;
			continue;
		}

		if (!((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) &&
			family.queueFlags & VK_QUEUE_COMPUTE_BIT))
		{
			++i;
			continue;
		}

		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(m_device, i, m_surface, &present_support);

		if (present_support)
			return i;

		++i;
	}
	assert(false);
}

void base::create_logical_device()
{
	/*std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::set<int> unique_queue_families = { m_queue_family_indices.graphics_family, m_queue_family_indices.present_family,
		m_queue_family_indices.compute_family};

	float queue_priority = 1.0f;

	for (int queue_family : unique_queue_families)
	{
		VkDeviceQueueCreateInfo queue_create_info = {};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}*/

	float queue_priorities[QUEUE_COUNT];
	queue_priorities[QUEUE_RENDER] = 1.f;
	queue_priorities[QUEUE_COMPUTE] = 1.f;
	queue_priorities[QUEUE_RESOURCE_BUILD] = 0.7f;
	queue_priorities[QUEUE_TERRAIN_LOADER] = 0.5f;
	queue_priorities[QUEUE_PRESENT] = 1.f;

	VkDeviceQueueCreateInfo queue;
	queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue.queueFamilyIndex = m_queue_family_index;
	queue.queueCount = QUEUE_COUNT;
	queue.pQueuePriorities = queue_priorities;

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = &queue;
	create_info.queueCreateInfoCount = 1;
	create_info.pEnabledFeatures = &m_info.device_features;
	create_info.ppEnabledExtensionNames = m_info.device_extensions;
	create_info.enabledExtensionCount = m_info.device_extensions_count;

	if (m_info.enable_validation_layers)
	{
		create_info.enabledLayerCount = m_info.validation_layer_count;
		create_info.ppEnabledLayerNames = m_info.validation_layers;
	}
	else
		create_info.enabledLayerCount = 0;

	assert(vkCreateDevice(m_physical_device, &create_info, m_vk_alloc, &m_device) == VK_SUCCESS);

	for (uint32_t i = 0; i < QUEUE_COUNT; ++i)
		vkGetDeviceQueue(m_device, m_queue_family_index, i, m_queues + i);
}

void base::create_swapchain()
{
	
	//pick present mode
	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, nullptr);
	assert(present_mode_count != 0);
	VkPresentModeKHR present_modes[VK_PRESENT_MODE_RANGE_SIZE_KHR];
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, present_modes);
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	for (auto modes : present_modes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			present_mode = mode;
			break;
		}
		else if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			present_mode = mode;
	}


	//check if swapchain parameters are supported
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &capabilities);
	assert(capabilities.minImageCount <= SWAP_CHAIN_IMAGE_COUNT);
	if (capabilities.maxImageCount != 0)
		assert(capabilities.maxImageCount >= SWAP_CHAIN_IMAGE_COUNT);

	assert(capabilities.minImageExtent.width <= SWAP_CHAIN_IMAGE_EXTENT.width &&
		capabilities.minImageExtent.height <= SWAP_CHAIN_IMAGE_EXTENT.height &&
		capabilities.maxImageExtent.width >= SWAP_CHAIN_IMAGE_EXTENT.width &&
		capabilities.maxImageExtent.height >= SWAP_CHAIN_IMAGE_EXTENT.height);


	//fill create info
	VkSwapchainCreateInfoKHR sc = {};
	sc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	sc.surface = m_surface;
	sc.minImageCount = SWAP_CHAIN_IMAGE_COUNT;
	sc.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
	sc.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	sc.imageExtent = SWAP_CHAIN_IMAGE_EXTENT;
	sc.presentMode = present_mode;
	sc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	sc.imageArrayLayers = 1;
	sc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	/*uint32_t family_indices[] = { static_cast<uint32_t>(m_queue_family_indices.graphics_family),
		static_cast<uint32_t>(m_queue_family_indices.present_family) };

	if (m_queue_family_indices.graphics_family == m_queue_family_indices.present_family)
	{*/
	sc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sc.queueFamilyIndexCount = 0;
	sc.pQueueFamilyIndices = nullptr;
	/*}
	else
	{
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = family_indices;
	}*/

	sc.preTransform = capabilities.currentTransform;
	sc.clipped = VK_TRUE;
	sc.oldSwapchain = VK_NULL_HANDLE;

	assert(vkCreateSwapchainKHR(m_device, &create_info, m_vk_alloc, &m_swapchain) == VK_SUCCESS);

	uint32_t image_count;
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
	assert(image_count == SWAP_CHAIN_IMAGE_COUNT);
	vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, m_swapchain_images);
}

void base::create_swapchain_views()
{
	VkImageViewCreateInfo view = {};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.format = m_swap_chin_image_format;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;

	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view.subresourceRange.baseArrayLayer = 0;
	view.subresourceRange.baseMipLevel = 0;
	view.subresourceRange.layerCount = 1;
	view.subresourceRange.levelCount = 1;

	for (uint32_t i = 0; i < SWAP_CHAIN_IMAGE_COUNT; ++i)
	{
		view.image = m_swapchain_images[i];
		assert(vkCreateImageView(m_device, &view, m_alloc, m_swapchain_views+i) == VK_SUCCESS);
	}
}

void base::fill_base_info()
{
	m_base_info.device = m_device;
	m_base_info.physical_device = m_physical_device;
	m_base_info.queue_family_index = m_queue_family_index;
	m_base_info.swapchain = m_swapchain;
	m_base_info.swapchain_views = m_swapchain_views;
	m_base_info.window = m_window;
	for (uint32_t i = 0; i < QUEUE_COUNT; ++i)
		m_base_info.queues[i] = m_queues[i];
}



/*bool base::check_device_extension_support(VkPhysicalDevice device)
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
}*/




/*VkSurfaceFormatKHR base::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& formats)
{
if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

for (const auto& available_format : formats)
{
if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
return available_format;
}
assert(false);// return formats[0];
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
}*/
/*VkExtent2D base::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
{
if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
return capabilities.currentExtent;

int width, height;
glfwGetWindowSize(m_window, &width, &height);

VkExtent2D actual_extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(actual_extent.width, capabilities.maxImageExtent.width));
actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(actual_extent.height, capabilities.maxImageExtent.height));

return actual_extent;
}*/

/*bool base::device_is_suitable(VkPhysicalDevice device)
{
if (!check_device_extension_support(device) || !find_queue_family_index(device).complete())
return false;


swap_chain_support_details swap_chain_support = query_swap_chain_support_details(device);
bool swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();

if (!swap_chain_adequate)
return false;

VkPhysicalDeviceFeatures device_features;
vkGetPhysicalDeviceFeatures(device, &device_features);


VkPhysicalDeviceProperties device_properties;
vkGetPhysicalDeviceProperties(device, &device_properties);

return true;
}*/

/*swap_chain_support_details base::query_swap_chain_support_details(VkPhysicalDevice device)
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
}*/