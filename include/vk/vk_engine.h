#ifndef _VK_ENGINE_H_
#define _VK_ENGINE_H_

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "vk/vk_types.h"
#include <cglm/cglm.h>

static const int MAX_FRAMES_IN_FLIGHT = 2;

static VkResult
CreateDebugUtilsMessengerEXT(VkInstance instance,
			     const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
			     const VkAllocationCallbacks *pAllocator,
			     VkDebugUtilsMessengerEXT *pDebugMessenger)
{
	PFN_vkCreateDebugUtilsMessengerEXT func =
		(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
			instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != NULL) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
					  VkDebugUtilsMessengerEXT debugMessenger,
					  const VkAllocationCallbacks *pAllocator)
{
	PFN_vkDestroyDebugUtilsMessengerEXT func =
		(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
			instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != NULL) {
		func(instance, debugMessenger, pAllocator);
	}
}

typedef struct {
	vec4 pos;
	vec3 color;
} vertex;

typedef struct {
	Uint32 graphics_family;
	bool graphics_found;
	Uint32 present_family;
	bool present_found;
} queue_family_indices;

typedef struct {
	VkSurfaceCapabilitiesKHR capabilites;
	Uint32 format_size;
	Uint32 present_mode_size;
	VkSurfaceFormatKHR *formats;
	VkPresentModeKHR *present_modes;
} swap_chain_support_details;

typedef struct {
	VkExtent2D swap_chain_extent;
	VkFormat swap_chain_image_format;
	Uint32 frame_num;
	Uint32 swap_chain_images_size;
	Uint32 current_frame;
	bool initialized;
	bool fb_resized_flag;
	VkPipeline graphics_pipeline;
	VkRenderPass render_pass;
	VkCommandBuffer *command_buffers;
	VkSemaphore *image_avail_sems;
	VkSemaphore *rend_finished_sems;
	VkFence *in_flight_fences;
	VkPipelineLayout pipeline_layout;
	VkSurfaceKHR sdl_surface;
	VkQueue graphics_queue;
	VkQueue present_queue;
	VkInstance vk_instance;
	VkSwapchainKHR swap_chain;
	VkImage *swap_chain_images;
	VkImageView *swap_chain_image_views;
	VkCommandPool command_pool;
	VkFramebuffer *swap_chain_frame_buffers;
	VkPhysicalDevice phy_dev;
	VkDevice log_dev;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkExtent2D win_extent;
	SDL_Window *win;
} vulkan_engine;
void vulkan_engine_draw_frame(vulkan_engine *self);
void vulkan_engine_init(vulkan_engine *self, SDL_Window *window);
void vulkan_engine_cleanup(vulkan_engine *self);
void vulkan_engine_recreate_swap_chain(vulkan_engine *self);

#endif // !_VK_ENGINE_H_
