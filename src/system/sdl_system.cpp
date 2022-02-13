//
// Created by ethan on 2/5/22.
//

#include "sdl_system.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

SDL_Event *SystemEvent::GetInternalEvent() {
    return &(this->sdl_event);
}

SystemEventType SystemEvent::GetType() {
    uint32_t event_type = this->GetInternalEvent()->type;
    return (SystemEventType)event_type;
}

SystemWindow::SystemWindow(const char *_title, int _width, int _height) {
    this->title = _title;
    this->width = _width;
    this->height = _height;
}

void SystemWindow::Initialize() {
    this->window = SDL_CreateWindow(
            this->title,
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            this->width, this->height,
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

SystemWindow SystemSDL::CreateWindow(const char *title, int width, int height) {
    SystemWindow window(title, width, height);
    window.Initialize();
    return window;
}

int SystemSDL::PollEvent(SystemEvent *result) {
    return SDL_PollEvent(result->GetInternalEvent());
}

SystemWindow SystemSDL::GetCurrentWindow() {
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
