#include <stdlib.h>
#include "vk/vk_engine.h"

#define SCREEN_WIDTH 1700
#define SCREEN_HEIGHT 900

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WindowFlags win_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
	SDL_Window *win = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
					   SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
					   SCREEN_HEIGHT, win_flags);

	vulkan_engine engine;
	vulkan_engine_init(&engine, win);

	SDL_Event e;
	bool quit = false;
	bool minimized = false;
	while (!quit) {
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) {
				quit = true;
			} else if (e.type == SDL_WINDOWEVENT) {
				// int newWidth = e.window.data1;
				// int newHeight = e.window.data2;
				switch (e.window.event) {
				case SDL_WINDOWEVENT_RESIZED:
					engine.fb_resized_flag = true;
					break;
				case SDL_WINDOWEVENT_MINIMIZED:
					minimized = true;
					break;
				case SDL_WINDOWEVENT_RESTORED:
					minimized = false;
					break;
				}
			}
		}
		if (!minimized) {
			vulkan_engine_draw_frame(&engine);
		}
	}

	vulkan_engine_cleanup(&engine);
	SDL_DestroyWindow(win);
	return EXIT_SUCCESS;
}
