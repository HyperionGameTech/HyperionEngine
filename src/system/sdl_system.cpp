//
// Created by ethan on 2/5/22.
//

#include "sdl_system.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

SystemWindow::SystemWindow(const char *title, int width, int height) {
    this->window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
    );
    if (this->window == nullptr) {
        // TODO: change this to use error logging
        throw std::runtime_error("Error creating SDL Window!");
    }
}

SDL_Window *SystemWindow::GetSDLWindow() {
    return this->window;
}

SystemWindow::~SystemWindow() {
    SDL_DestroyWindow(this->window);
}


SystemSDL::SystemSDL() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

void SystemSDL::SetCurrentWindow(const SystemWindow &window) {
    this->current_window = window;
}

SystemWindow SystemSDL::GetCurrentWindow(void) {
    return this->current_window;
}

std::vector<const char *> SystemSDL::GetVulkanExtensionNames() {
    uint32_t vk_ext_count = 0;
    auto *window = this->current_window.GetSDLWindow();

    SDL_Vulkan_GetInstanceExtensions(window, &vk_ext_count, nullptr);
    std::vector<const char *> extensions(vk_ext_count);
    SDL_Vulkan_GetInstanceExtensions(window, &vk_ext_count, extensions.data());

    return extensions;
}

SystemSDL::~SystemSDL() {
    SDL_Quit();
}
