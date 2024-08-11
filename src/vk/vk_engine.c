#include "vk/vk_engine.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <string.h>
#include <stddef.h>

#include "file.h"

#define SCREEN_WIDTH 1700
#define SCREEN_HEIGHT 900

static const char *validation_layers[] = {
	"VK_LAYER_KHRONOS_validation",
};

static const char *device_extensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
static const Uint32 device_extensions_size = 1;

#ifdef DEBUG
const bool enable_validation_layers = true;
#else
const bool enable_validation_layers = false;
#endif /* ifdef DEBUG */

const vertex VERTICES[3] = { { { 0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
			     { { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
			     { { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } } };

static VkVertexInputBindingDescription vertex_binding_description()
{
	VkVertexInputBindingDescription bind_desc;
	bind_desc.binding = 0;
	bind_desc.stride = sizeof(vertex);
	bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bind_desc;
}
// Make sure to free returned array
static VkVertexInputAttributeDescription *vertex_attribute_description()
{
	VkVertexInputAttributeDescription *attr_descs =
		malloc(sizeof(VkVertexInputAttributeDescription) * 2);

	attr_descs[0].binding = 0;
	attr_descs[0].location = 0;
	attr_descs[0].format = VK_FORMAT_R32G32_SFLOAT;
	attr_descs[0].offset = offsetof(vertex, pos);

	attr_descs[1].binding = 0;
	attr_descs[1].location = 1;
	attr_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attr_descs[1].offset = offsetof(vertex, color);

	return attr_descs;
}

static Uint32 find_memory_flags(vulkan_engine *self, Uint32 type_filter,
				VkMemoryPropertyFlags props)
{
	VkPhysicalDeviceMemoryProperties mem_props;
	vkGetPhysicalDeviceMemoryProperties(self->phy_dev, &mem_props);

	for (Uint32 i = 0; i < mem_props.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) &&
		    (mem_props.memoryTypes[i].propertyFlags & props) == props) {
			return i;
		}
	}

	return -1;
}

static bool check_validation_layer_support()
{
	Uint32 layer_cnt;
	vkEnumerateInstanceLayerProperties(&layer_cnt, NULL);

	VkLayerProperties layer_props[layer_cnt];
	vkEnumerateInstanceLayerProperties(&layer_cnt, layer_props);
	Uint32 vl_cnt = 1;

	for (int i = 0; i < vl_cnt; i++) {
		bool layer_found = false;
		for (int j = 0; j < layer_cnt; j++) {
			if (strcmp(validation_layers[i], layer_props[j].layerName) ==
			    0) {
				layer_found = true;
				break;
			}
		}
		if (!layer_found) {
			return false;
		}
	}

	return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
	fprintf(stderr, "VL_DEBUG: %s\n", pCallbackData->pMessage);

	return VK_FALSE;
}

static void
populate_debug_messenger_creat_info(VkDebugUtilsMessengerCreateInfoEXT *creat_info)
{
	creat_info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	creat_info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	creat_info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	creat_info->pfnUserCallback = debugCallback;
	creat_info->pUserData = NULL;
	creat_info->pNext = NULL;
	creat_info->flags = 0;
}

static void create_instance(vulkan_engine *self)
{
	self->win_extent.width = SCREEN_WIDTH;
	self->win_extent.height = SCREEN_HEIGHT;
	self->initialized = true;
	self->frame_num = 0;
	self->phy_dev = VK_NULL_HANDLE;
	self->sdl_surface = NULL;
	self->graphics_queue = VK_NULL_HANDLE;
	self->present_queue = VK_NULL_HANDLE;
	self->swap_chain = VK_NULL_HANDLE;
	self->swap_chain_images = NULL;
	self->swap_chain_images_size = 0;
	self->swap_chain_image_views = NULL;
	self->render_pass = NULL;
	self->pipeline_layout = NULL;
	self->current_frame = 0;
	self->fb_resized_flag = false;

	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Vukan Engine",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 3),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 3),
		.apiVersion = VK_API_VERSION_1_3,
		.pNext = NULL,
	};

	uint32_t ext_count = 0;
	SDL_Vulkan_GetInstanceExtensions(self->win, &ext_count, NULL);

	const char *ext_names[ext_count + 1];
	SDL_Vulkan_GetInstanceExtensions(self->win, &ext_count, ext_names);

	if (enable_validation_layers) {
		ext_names[ext_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	}

	printf("extenions:\n");
	for (int i = 0; i < ext_count; i++) {
		printf("\tusing: %s\n", ext_names[i]);
	}

	VkInstanceCreateInfo creat_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.flags = 0,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = NULL,
		.ppEnabledExtensionNames = ext_names,
		.enabledExtensionCount = ext_count,
	};
	if (enable_validation_layers && !check_validation_layer_support()) {
		fprintf(stderr, "Validation layers requested, but not available\n");
		return;
	}
	VkDebugUtilsMessengerCreateInfoEXT debug_creat_info;
	if (enable_validation_layers) {
		creat_info.enabledLayerCount = 1;
		creat_info.ppEnabledLayerNames = validation_layers;

		populate_debug_messenger_creat_info(&debug_creat_info);
		creat_info.pNext = &debug_creat_info;
	} else {
		creat_info.enabledLayerCount = 0;
		creat_info.pNext = NULL;
	}

	VkResult res = vkCreateInstance(&creat_info, NULL, &self->vk_instance);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "Error creating vkinstance err: %s\n",
			string_VkResult(res));
	}
}

static void setup_debug_messenger(vulkan_engine *self)
{
	if (!enable_validation_layers)
		return;

	VkDebugUtilsMessengerCreateInfoEXT creat_info;
	populate_debug_messenger_creat_info(&creat_info);

	if (CreateDebugUtilsMessengerEXT(self->vk_instance, &creat_info, NULL,
					 &self->debug_messenger) != VK_SUCCESS) {
		fprintf(stderr, "failed to set up debug messenger!\n");
		return;
	}
}

static void queue_family_indices_init(VkPhysicalDevice dev, VkSurfaceKHR surface,
				      queue_family_indices *r_indices)
{
	memset(r_indices, 0, sizeof(queue_family_indices));
	Uint32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_count, NULL);

	VkQueueFamilyProperties queue_families[queue_family_count];
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_count,
						 queue_families);

	for (Uint32 i = 0; i < queue_family_count; i++) {
		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			r_indices->graphics_family = i;
			r_indices->graphics_found = true;
		}
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present_support);
		if (present_support) {
			r_indices->present_family = i;
			r_indices->present_found = true;
		}
	}
}

static bool queue_family_indices_is_complete(queue_family_indices *indices)
{
	return indices->graphics_found && indices->present_found;
}

static bool check_dev_ext_support(VkPhysicalDevice dev)
{
	Uint32 ext_count;
	vkEnumerateDeviceExtensionProperties(dev, NULL, &ext_count, NULL);

	VkExtensionProperties exts[ext_count];
	vkEnumerateDeviceExtensionProperties(dev, NULL, &ext_count, exts);

	for (Uint32 i = 0; i < device_extensions_size; i++) {
		const char *ext = device_extensions[i];
		bool found = false;
		for (Uint32 j = 0; j < ext_count; j++) {
			if (strcmp(ext, exts[j].extensionName) == 0) {
				found = true;
			}
		}
		if (!found) {
			fprintf(stderr, "Physical devices is missing support for %s\n",
				ext);
			return false;
		}
	}

	return true;
}

static void swap_chain_support_details_init(VkPhysicalDevice phy_dev,
					    VkSurfaceKHR sdl_surface,
					    swap_chain_support_details *rdetails)
{
	// set up
	memset(rdetails, 0, sizeof(swap_chain_support_details));
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_dev, sdl_surface,
						  &rdetails->capabilites);

	// allocate formats
	vkGetPhysicalDeviceSurfaceFormatsKHR(phy_dev, sdl_surface,
					     &rdetails->format_size, NULL);
	if (rdetails->format_size != 0) {
		rdetails->formats =
			malloc(sizeof(VkSurfaceFormatKHR) * rdetails->format_size);
		vkGetPhysicalDeviceSurfaceFormatsKHR(phy_dev, sdl_surface,
						     &rdetails->format_size,
						     rdetails->formats);
	}

	// allocate present modes
	vkGetPhysicalDeviceSurfacePresentModesKHR(phy_dev, sdl_surface,
						  &rdetails->present_mode_size, NULL);
	if (rdetails->present_mode_size != 0) {
		rdetails->present_modes =
			malloc(sizeof(VkPresentModeKHR) * rdetails->present_mode_size);
		vkGetPhysicalDeviceSurfacePresentModesKHR(phy_dev, sdl_surface,
							  &rdetails->present_mode_size,
							  rdetails->present_modes);
	}
}

static void swap_chain_support_details_destroy(swap_chain_support_details *details)
{
	free(details->present_modes);
	free(details->formats);
}

static bool phy_dev_is_usable(VkPhysicalDevice dev, VkSurfaceKHR surface)
{
	queue_family_indices family_indices;
	queue_family_indices_init(dev, surface, &family_indices);

	bool extensions_supported = check_dev_ext_support(dev);

	bool swap_chain_adequate = false;
	if (extensions_supported) {
		swap_chain_support_details details;
		swap_chain_support_details_init(dev, surface, &details);

		swap_chain_adequate = details.present_mode_size != 0 &&
				      details.format_size != 0;
		swap_chain_support_details_destroy(&details);
	}

	return queue_family_indices_is_complete(&family_indices) &&
	       extensions_supported && swap_chain_adequate;
}

static void pick_phy_device(vulkan_engine *self)
{
	Uint32 dev_count = 0;
	vkEnumeratePhysicalDevices(self->vk_instance, &dev_count, NULL);
	if (dev_count == 0) {
		fprintf(stderr, "Failed to find vulkan supporting GPUs");
		return;
	}

	VkPhysicalDevice devices[dev_count];
	vkEnumeratePhysicalDevices(self->vk_instance, &dev_count, devices);

	for (int i = 0; i < dev_count; i++) {
		if (phy_dev_is_usable(devices[i], self->sdl_surface)) {
			self->phy_dev = devices[i];
			break;
		}
	}

	if (self->phy_dev == NULL) {
		fprintf(stderr, "engine's phy_dev is NULL, no usable GPU found\n");
	}
}

static void create_logical_device(vulkan_engine *self)
{
	queue_family_indices family_indices;
	queue_family_indices_init(self->phy_dev, self->sdl_surface, &family_indices);

	if (!queue_family_indices_is_complete(&family_indices)) {
		fprintf(stderr, "unable to find queue families for physical device\n");
		return;
	}
	//
	// annoying set loop
	//
	Uint32 family_indices_to_use[2] = {
		family_indices.graphics_family,
		family_indices.present_family,
	};
	Uint32 family_indices_used[2] = { 0 };
	Uint32 used_idx = 0;
	for (Uint32 i = 0; i < 2; i++) {
		Uint32 ind = family_indices_to_use[i];
		bool exist = false;
		for (Uint32 j = 0; j < 2; j++) {
			if (ind == family_indices_used[j]) {
				exist = true;
			}
		}
		if (!exist) {
			family_indices_used[used_idx++] = ind;
		}
	}
	//
	// end of annoying set loop
	//
	VkDeviceQueueCreateInfo queue_create_infos[used_idx + 1];

	float queue_priority = 1.0f;
	for (Uint32 i = 0; i < used_idx + 1; i++) {
		VkDeviceQueueCreateInfo queue_creat_info = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = family_indices_used[i],
			.queueCount = 1,
			.pNext = NULL,
			.flags = 0,
			.pQueuePriorities = &queue_priority,
		};
		queue_create_infos[i] = queue_creat_info;
	}

	VkPhysicalDeviceFeatures feats;
	SDL_memset(&feats, 0, sizeof(VkPhysicalDeviceFeatures));

	VkDeviceCreateInfo dev_creat_info;
	SDL_memset(&dev_creat_info, 0, sizeof(VkDeviceCreateInfo));
	dev_creat_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dev_creat_info.pQueueCreateInfos = queue_create_infos;
	dev_creat_info.queueCreateInfoCount = used_idx + 1;
	dev_creat_info.pEnabledFeatures = &feats;
	dev_creat_info.enabledExtensionCount = device_extensions_size;
	dev_creat_info.ppEnabledExtensionNames = device_extensions;

	if (enable_validation_layers) {
		dev_creat_info.enabledLayerCount = 1;
		dev_creat_info.ppEnabledLayerNames = validation_layers;
	} else {
		dev_creat_info.enabledLayerCount = 0;
	}
	VkResult result =
		vkCreateDevice(self->phy_dev, &dev_creat_info, NULL, &self->log_dev);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error creating logical dev err: %s\n",
			string_VkResult(result));
	}
	vkGetDeviceQueue(self->log_dev, family_indices.graphics_family, 0,
			 &self->graphics_queue);
	vkGetDeviceQueue(self->log_dev, family_indices.present_family, 0,
			 &self->present_queue);
}

static void create_surface(vulkan_engine *self)
{
	if (!SDL_Vulkan_CreateSurface(self->win, self->vk_instance,
				      &self->sdl_surface)) {
		fprintf(stderr,
			"Failed to create sdl surface from vulkan instance err: %s\n",
			SDL_GetError());
	}
}

VkSurfaceFormatKHR choose_swap_surface_format(VkSurfaceFormatKHR *formats,
					      Uint32 formats_size)
{
	for (Uint32 i = 0; i < formats_size; i++) {
		if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
		    formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return formats[i];
		}
	}

	return formats[0];
}

VkPresentModeKHR choose_swap_present_mode(VkPresentModeKHR *present_modes,
					  Uint32 present_modes_size)
{
	for (Uint32 i = 0; i < present_modes_size; i++) {
		if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return present_modes[i];
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swap_extent(VkSurfaceCapabilitiesKHR *capabilites, SDL_Window *win)
{
	if (capabilites->currentExtent.width != UINT32_MAX) {
		return capabilites->currentExtent;
	} else {
		int32_t width;
		int32_t height;
		SDL_GL_GetDrawableSize(win, &width, &height);

		VkExtent2D actual_extent = {
			.width = width,
			.height = height,
		};

		glm_clamp(actual_extent.width, capabilites->minImageExtent.width,
			  capabilites->maxImageExtent.width);
		glm_clamp(actual_extent.height, capabilites->minImageExtent.height,
			  capabilites->maxImageExtent.height);
		return actual_extent;
	}
}

static void create_swap_chain(vulkan_engine *self)
{
	swap_chain_support_details details;
	swap_chain_support_details_init(self->phy_dev, self->sdl_surface, &details);

	VkSurfaceFormatKHR format =
		choose_swap_surface_format(details.formats, details.format_size);

	VkPresentModeKHR present_mode = choose_swap_present_mode(
		details.present_modes, details.present_mode_size);

	VkExtent2D extent = choose_swap_extent(&details.capabilites, self->win);
	// recommened to request more than 1 over min
	Uint32 image_count = details.capabilites.minImageCount + 1;

	if (details.capabilites.maxImageCount > 0 &&
	    image_count > details.capabilites.maxImageCount) {
		image_count = details.capabilites.maxImageCount;
	}

	queue_family_indices indices;
	queue_family_indices_init(self->phy_dev, self->sdl_surface, &indices);
	Uint32 families[] = { indices.graphics_family, indices.present_family };

	VkSwapchainCreateInfoKHR sc_creat;
	memset(&sc_creat, 0, sizeof(VkSwapchainCreateInfoKHR));
	sc_creat.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	sc_creat.surface = self->sdl_surface;
	sc_creat.minImageCount = image_count;
	sc_creat.imageFormat = format.format;
	sc_creat.imageColorSpace = format.colorSpace;
	sc_creat.imageExtent = extent;
	sc_creat.imageArrayLayers = 1;
	sc_creat.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if (indices.graphics_family != indices.present_family) {
		sc_creat.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		sc_creat.queueFamilyIndexCount = 2;
		sc_creat.pQueueFamilyIndices = families;
	} else {
		sc_creat.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		sc_creat.queueFamilyIndexCount = 0;
		sc_creat.pQueueFamilyIndices = NULL;
	}

	sc_creat.preTransform = details.capabilites.currentTransform;
	sc_creat.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	sc_creat.presentMode = present_mode;
	sc_creat.clipped = true;
	sc_creat.oldSwapchain = NULL;

	VkResult result =
		vkCreateSwapchainKHR(self->log_dev, &sc_creat, NULL, &self->swap_chain);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error creating swap_chain. err: %s\n",
			string_VkResult(result));
	}
	// set up swap chain images
	vkGetSwapchainImagesKHR(self->log_dev, self->swap_chain,
				&self->swap_chain_images_size, NULL);
	self->swap_chain_images =
		malloc(sizeof(VkImage) * self->swap_chain_images_size);
	vkGetSwapchainImagesKHR(self->log_dev, self->swap_chain,
				&self->swap_chain_images_size, self->swap_chain_images);

	self->swap_chain_extent = extent;
	self->swap_chain_image_format = format.format;

	swap_chain_support_details_destroy(&details);
}

void create_image_views(vulkan_engine *self)
{
	self->swap_chain_image_views =
		malloc(sizeof(VkImageView) * self->swap_chain_images_size);

	for (Uint32 i = 0; i < self->swap_chain_images_size; i++) {
		VkImageViewCreateInfo iv_creat;
		memset(&iv_creat, 0, sizeof(VkImageViewCreateInfo));

		iv_creat.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		iv_creat.image = self->swap_chain_images[i];
		iv_creat.viewType = VK_IMAGE_VIEW_TYPE_2D;
		iv_creat.format = self->swap_chain_image_format;
		iv_creat.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		iv_creat.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		iv_creat.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		iv_creat.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		iv_creat.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		iv_creat.subresourceRange.baseMipLevel = 0;
		iv_creat.subresourceRange.levelCount = 1;
		iv_creat.subresourceRange.baseArrayLayer = 0;
		iv_creat.subresourceRange.layerCount = 1;

		VkResult result = vkCreateImageView(self->log_dev, &iv_creat, NULL,
						    &self->swap_chain_image_views[i]);
		if (result != VK_SUCCESS) {
			fprintf(stderr, "Error creating image view. err: %s\n",
				string_VkResult(result));
		}
	}
}

VkShaderModule create_shader_module(VkDevice log_dev, loaded_file *lf)
{
	VkShaderModuleCreateInfo smci;
	memset(&smci, 0, sizeof(VkShaderModuleCreateInfo));

	smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smci.codeSize = lf->size;
	smci.pCode = (Uint32 *)lf->buf;

	VkShaderModule module;
	VkResult result = vkCreateShaderModule(log_dev, &smci, NULL, &module);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error creating shader module. err: %s\n",
			string_VkResult(result));
	}

	return module;
}

void create_graphics_pipeline(vulkan_engine *self)
{
	// shader stage
	loaded_file vert_code = read_file("build/vert.spv");
	loaded_file frag_code = read_file("build/frag.spv");

	VkShaderModule vert_mod = create_shader_module(self->log_dev, &vert_code);
	VkShaderModule frag_mod = create_shader_module(self->log_dev, &frag_code);

	VkPipelineShaderStageCreateInfo pvssci;
	memset(&pvssci, 0, sizeof(VkPipelineShaderStageCreateInfo));
	pvssci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pvssci.stage = VK_SHADER_STAGE_VERTEX_BIT;
	pvssci.module = vert_mod;
	pvssci.pName = "main";

	VkPipelineShaderStageCreateInfo pfssci;
	memset(&pfssci, 0, sizeof(VkPipelineShaderStageCreateInfo));
	pfssci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pfssci.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pfssci.module = frag_mod;
	pfssci.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { pvssci, pfssci };

	// dynamic stage

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dsci;
	memset(&dsci, 0, sizeof(VkPipelineDynamicStateCreateInfo));
	dsci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dsci.dynamicStateCount = 2;
	dsci.pDynamicStates = dynamic_states;

	VkVertexInputBindingDescription vertex_bind_desc = vertex_binding_description();
	VkVertexInputAttributeDescription *vertex_attr_decs =
		vertex_attribute_description();

	VkPipelineVertexInputStateCreateInfo visci;
	memset(&visci, 0, sizeof(VkPipelineVertexInputStateCreateInfo));
	visci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	visci.vertexBindingDescriptionCount = 1;
	visci.vertexAttributeDescriptionCount = 2; //TODO hard coded baby

	visci.pVertexBindingDescriptions = &vertex_bind_desc;
	visci.pVertexAttributeDescriptions = vertex_attr_decs;

	VkPipelineInputAssemblyStateCreateInfo iaci;
	memset(&iaci, 0, sizeof(VkPipelineInputAssemblyStateCreateInfo));
	iaci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	iaci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	iaci.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport;
	memset(&viewport, 0, sizeof(VkViewport));
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = self->swap_chain_extent.width;
	viewport.height = self->swap_chain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = self->swap_chain_extent;

	VkPipelineViewportStateCreateInfo vsci;
	memset(&vsci, 0, sizeof(VkPipelineViewportStateCreateInfo));
	vsci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vsci.viewportCount = 1;
	vsci.pViewports = &viewport;
	vsci.scissorCount = 1;
	vsci.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rastci;
	memset(&rastci, 0, sizeof(VkPipelineRasterizationStateCreateInfo));
	rastci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rastci.lineWidth = 1.0f;
	rastci.depthClampEnable = VK_FALSE;
	rastci.polygonMode = VK_POLYGON_MODE_FILL;
	rastci.cullMode = VK_CULL_MODE_BACK_BIT;
	rastci.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rastci.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisci;
	memset(&multisci, 0, sizeof(VkPipelineMultisampleStateCreateInfo));
	multisci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisci.sampleShadingEnable = VK_FALSE;
	multisci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisci.minSampleShading = 1.0f;
	multisci.pSampleMask = NULL;
	multisci.alphaToCoverageEnable = VK_FALSE;
	multisci.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState color_blend_att;
	color_blend_att.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	color_blend_att.blendEnable = VK_TRUE;
	color_blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_att.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_att.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blend_ci;
	memset(&color_blend_ci, 0, sizeof(VkPipelineColorBlendStateCreateInfo));
	color_blend_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_ci.logicOpEnable = VK_FALSE;
	color_blend_ci.logicOp = VK_LOGIC_OP_COPY;
	color_blend_ci.attachmentCount = 1;
	color_blend_ci.pAttachments = &color_blend_att;
	color_blend_ci.blendConstants[0] = 0.0f;
	color_blend_ci.blendConstants[1] = 0.0f;
	color_blend_ci.blendConstants[2] = 0.0f;
	color_blend_ci.blendConstants[3] = 0.0f;

	memset(&self->pipeline_layout, 0, sizeof(VkPipelineLayout));

	VkPipelineLayoutCreateInfo pipeline_layout_ci;
	memset(&pipeline_layout_ci, 0, sizeof(VkPipelineLayoutCreateInfo));
	pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	VkResult result = vkCreatePipelineLayout(self->log_dev, &pipeline_layout_ci,
						 NULL, &self->pipeline_layout);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error creating pipeline layout. err: %s\n",
			string_VkResult(result));
	}

	VkGraphicsPipelineCreateInfo pipeline_ci;
	memset(&pipeline_ci, 0, sizeof(VkGraphicsPipelineCreateInfo));
	pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_ci.stageCount = 2;
	pipeline_ci.pStages = shader_stages;
	pipeline_ci.pVertexInputState = &visci;
	pipeline_ci.pInputAssemblyState = &iaci;
	pipeline_ci.pViewportState = &vsci;
	pipeline_ci.pRasterizationState = &rastci;
	pipeline_ci.pMultisampleState = &multisci;
	pipeline_ci.pDepthStencilState = NULL;
	pipeline_ci.pColorBlendState = &color_blend_ci;
	pipeline_ci.pDynamicState = &dsci;
	pipeline_ci.layout = self->pipeline_layout;
	pipeline_ci.renderPass = self->render_pass;
	pipeline_ci.subpass = 0;
	pipeline_ci.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_ci.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(self->log_dev, VK_NULL_HANDLE, 1,
					   &pipeline_ci, NULL,
					   &self->graphics_pipeline);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error creating graphics pipeline . err: %s\n",
			string_VkResult(result));
	}
	vkDestroyShaderModule(self->log_dev, vert_mod, NULL);
	vkDestroyShaderModule(self->log_dev, frag_mod, NULL);

	free(vertex_attr_decs);

	loaded_file_destroy(&vert_code);
	loaded_file_destroy(&frag_code);
}

static void create_render_pass(vulkan_engine *self)
{
	VkAttachmentDescription color_att;
	memset(&color_att, 0, sizeof(VkAttachmentDescription));
	color_att.format = self->swap_chain_image_format;
	color_att.samples = VK_SAMPLE_COUNT_1_BIT;
	color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_att_ref;
	memset(&color_att_ref, 0, sizeof(VkAttachmentReference));
	color_att_ref.attachment = 0;
	color_att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass;
	memset(&subpass, 0, sizeof(VkSubpassDescription));
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_att_ref;

	VkSubpassDependency dep;
	memset(&dep, 0, sizeof(VkSubpassDependency));
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.srcAccessMask = 0;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo rend_pass_ci;
	memset(&rend_pass_ci, 0, sizeof(VkRenderPassCreateInfo));
	rend_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rend_pass_ci.attachmentCount = 1;
	rend_pass_ci.pAttachments = &color_att;
	rend_pass_ci.subpassCount = 1;
	rend_pass_ci.pSubpasses = &subpass;
	rend_pass_ci.dependencyCount = 1;
	rend_pass_ci.pDependencies = &dep;

	VkResult result = vkCreateRenderPass(self->log_dev, &rend_pass_ci, NULL,
					     &self->render_pass);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error creating render pass. err: %s\n",
			string_VkResult(result));
	}
}

void create_frame_buffers(vulkan_engine *self)
{
	self->swap_chain_frame_buffers =
		malloc(sizeof(VkFramebuffer) * self->swap_chain_images_size);
	for (size_t i = 0; i < self->swap_chain_images_size; i++) {
		VkImageView attachments[] = { self->swap_chain_image_views[i] };

		VkFramebufferCreateInfo fb_ci;
		memset(&fb_ci, 0, sizeof(VkFramebufferCreateInfo));
		fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb_ci.renderPass = self->render_pass;
		fb_ci.attachmentCount = 1;
		fb_ci.pAttachments = attachments;
		fb_ci.width = self->swap_chain_extent.width;
		fb_ci.height = self->swap_chain_extent.height;
		fb_ci.layers = 1;
		VkResult result =
			vkCreateFramebuffer(self->log_dev, &fb_ci, NULL,
					    &self->swap_chain_frame_buffers[i]);
		if (result != VK_SUCCESS) {
			fprintf(stderr, "Error creating frame buffer. err: %s\n",
				string_VkResult(result));
		}
	}
}

void create_command_pool(vulkan_engine *self)
{
	queue_family_indices queue_family_indices;
	queue_family_indices_init(self->phy_dev, self->sdl_surface,
				  &queue_family_indices);

	if (!queue_family_indices.graphics_found) {
		fprintf(stderr, "Misisng graphics family queue\n");
		return;
	}

	VkCommandPoolCreateInfo cp_ci;
	memset(&cp_ci, 0, sizeof(VkCommandPoolCreateInfo));
	cp_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cp_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	cp_ci.queueFamilyIndex = queue_family_indices.graphics_family;

	VkResult result =
		vkCreateCommandPool(self->log_dev, &cp_ci, NULL, &self->command_pool);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error creating command pool, err: %s\n",
			string_VkResult(result));
	}
}

void create_command_buffers(vulkan_engine *self)
{
	self->command_buffers = malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo buf_info;
	buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	buf_info.commandPool = self->command_pool;
	buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	buf_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	buf_info.pNext = NULL;

	VkResult result = vkAllocateCommandBuffers(self->log_dev, &buf_info,
						   self->command_buffers);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error allocating command buffer, err: %s\n",
			string_VkResult(result));
	}
}

void record_command_buffer(vulkan_engine *self, VkCommandBuffer buffer,
			   Uint32 image_idx)
{
	VkCommandBufferBeginInfo begin_info;
	memset(&begin_info, 0, sizeof(VkCommandBufferBeginInfo));
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkResult result;
	result = vkBeginCommandBuffer(buffer, &begin_info);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error begin command buffer, err: %s\n",
			string_VkResult(result));
	}

	VkRenderPassBeginInfo rend_info;
	memset(&rend_info, 0, sizeof(VkRenderPassBeginInfo));
	rend_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rend_info.renderPass = self->render_pass;
	rend_info.framebuffer = self->swap_chain_frame_buffers[image_idx];
	rend_info.renderArea.extent = self->swap_chain_extent;
	rend_info.renderArea.offset.x = 0;
	rend_info.renderArea.offset.y = 0;

	VkClearValue clear = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
	rend_info.clearValueCount = 1;
	rend_info.pClearValues = &clear;

	vkCmdBeginRenderPass(buffer, &rend_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  self->graphics_pipeline);

	VkViewport viewport;
	memset(&viewport, 0, sizeof(VkViewport));
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)self->swap_chain_extent.width;
	viewport.height = (float)self->swap_chain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(buffer, 0, 1, &viewport);

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = self->swap_chain_extent;
	vkCmdSetScissor(buffer, 0, 1, &scissor);

	vkCmdDraw(buffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(buffer);

	result = vkEndCommandBuffer(buffer);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error with recording command buffer, err: %s",
			string_VkResult(result));
	}
}

void create_sync_objects(vulkan_engine *self)
{
	self->image_avail_sems = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
	self->rend_finished_sems = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
	self->in_flight_fences = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo sem_info;
	memset(&sem_info, 0, sizeof(VkSemaphoreCreateInfo));
	sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fen_info;
	memset(&fen_info, 0, sizeof(VkFenceCreateInfo));
	fen_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fen_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(self->log_dev, &sem_info, NULL,
				      &self->image_avail_sems[i]) != VK_SUCCESS ||
		    vkCreateSemaphore(self->log_dev, &sem_info, NULL,
				      &self->rend_finished_sems[i]) != VK_SUCCESS ||
		    vkCreateFence(self->log_dev, &fen_info, NULL,
				  &self->in_flight_fences[i]) != VK_SUCCESS) {
			fprintf(stderr, "Error creating sync objects\n");
		}
	}
}

void vulkan_engine_draw_frame(vulkan_engine *self)
{
	VkResult result;

	vkWaitForFences(self->log_dev, 1, &self->in_flight_fences[self->current_frame],
			VK_TRUE, UINT64_MAX);

	Uint32 image_idx;
	result = vkAcquireNextImageKHR(self->log_dev, self->swap_chain, UINT64_MAX,
				       self->image_avail_sems[self->current_frame],
				       NULL, &image_idx);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		vulkan_engine_recreate_swap_chain(self);
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		fprintf(stderr, "Error acquiring next image from swap chain, err: %s\n",
			string_VkResult(result));
	}

	vkResetFences(self->log_dev, 1, &self->in_flight_fences[self->current_frame]);

	vkResetCommandBuffer(self->command_buffers[self->current_frame], 0);

	record_command_buffer(self, self->command_buffers[self->current_frame],
			      image_idx);

	VkSubmitInfo submit_info;
	memset(&submit_info, 0, sizeof(VkSubmitInfo));
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_sems[] = { self->image_avail_sems[self->current_frame] };
	VkPipelineStageFlags wait_stages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_sems;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &self->command_buffers[self->current_frame];

	VkSemaphore signal_sems[] = { self->rend_finished_sems[self->current_frame] };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_sems;

	result = vkQueueSubmit(self->graphics_queue, 1, &submit_info,
			       self->in_flight_fences[self->current_frame]);
	if (result != VK_SUCCESS) {
		fprintf(stderr,
			"Error submitting command buffer to graphics queue, err: %s\n",
			string_VkResult(result));
	}

	VkPresentInfoKHR present_info;
	memset(&present_info, 0, sizeof(present_info));
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_sems;

	VkSwapchainKHR swap_chains[] = { self->swap_chain };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swap_chains;
	present_info.pImageIndices = &image_idx;

	result = vkQueuePresentKHR(self->present_queue, &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
	    self->fb_resized_flag) {
		self->fb_resized_flag = false;
		vulkan_engine_recreate_swap_chain(self);
	} else if (result != VK_SUCCESS) {
		fprintf(stderr, "Error with queing present, err: %s\n",
			string_VkResult(result));
	}

	self->current_frame = (self->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

static void create_vertex_buffer(vulkan_engine *self)
{
	VkBufferCreateInfo buf_info;
	memset(&buf_info, 1, sizeof(VkBufferCreateInfo));

	buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_info.size = sizeof(VERTICES[0]) * 3; // TODO hardcoded again baby
	buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result =
		vkCreateBuffer(self->log_dev, &buf_info, NULL, &self->vertex_buffer);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Error with queing present, err: %s\n",
			string_VkResult(result));
	}
}

void vulkan_engine_init(vulkan_engine *self, SDL_Window *window)
{
	self->win = window;
	create_instance(self);
	setup_debug_messenger(self);
	create_surface(self);
	pick_phy_device(self);
	create_logical_device(self);
	create_swap_chain(self);
	create_image_views(self);
	create_render_pass(self);
	create_graphics_pipeline(self);
	create_frame_buffers(self);
	create_command_pool(self);
	create_vertex_buffer(self);
	create_command_buffers(self);
	create_sync_objects(self);
}

void cleanup_swap_chain(vulkan_engine *self)
{
	for (size_t i = 0; i < self->swap_chain_images_size; i++) {
		vkDestroyFramebuffer(self->log_dev, self->swap_chain_frame_buffers[i],
				     NULL);
	}
	free(self->swap_chain_frame_buffers);
	for (Uint32 i = 0; i < self->swap_chain_images_size; i++) {
		vkDestroyImageView(self->log_dev, self->swap_chain_image_views[i],
				   NULL);
	}
	free(self->swap_chain_image_views);
	free(self->swap_chain_images);
	vkDestroySwapchainKHR(self->log_dev, self->swap_chain, NULL);
}

void vulkan_engine_recreate_swap_chain(vulkan_engine *self)
{
	vkDeviceWaitIdle(self->log_dev);

	cleanup_swap_chain(self);

	create_swap_chain(self);
	create_image_views(self);
	create_frame_buffers(self);
}

void vulkan_engine_cleanup(vulkan_engine *self)
{
	if (self->initialized) {
		vkDeviceWaitIdle(self->log_dev);
		cleanup_swap_chain(self);
		vkDestroyBuffer(self->log_dev, self->vertex_buffer, NULL);
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(self->log_dev, self->image_avail_sems[i],
					   NULL);
			vkDestroySemaphore(self->log_dev, self->rend_finished_sems[i],
					   NULL);
			vkDestroyFence(self->log_dev, self->in_flight_fences[i], NULL);
		}
		free(self->image_avail_sems);
		free(self->rend_finished_sems);
		free(self->in_flight_fences);
		vkDestroyCommandPool(self->log_dev, self->command_pool, NULL);
		free(self->command_buffers);
		vkDestroyRenderPass(self->log_dev, self->render_pass, NULL);
		vkDestroyPipeline(self->log_dev, self->graphics_pipeline, NULL);
		vkDestroyPipelineLayout(self->log_dev, self->pipeline_layout, NULL);
		vkDestroyDevice(self->log_dev, NULL);
		if (enable_validation_layers) {
			DestroyDebugUtilsMessengerEXT(self->vk_instance,
						      self->debug_messenger, NULL);
		}
		vkDestroySurfaceKHR(self->vk_instance, self->sdl_surface, NULL);
		vkDestroyInstance(self->vk_instance, NULL);
	}
}
