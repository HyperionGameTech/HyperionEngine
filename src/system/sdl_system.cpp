//
// Created by ethan on 2/5/22.
//

#include "sdl_system.h"
#include "debug.h"

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
    this->window = nullptr;
    this->title = _title;
    this->width = _width;
    this->height = _height;
}

void SystemWindow::Initialize() {
    this->window = SDL_CreateWindow(
            this->title,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            this->width, this->height,
            SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
    );

    AssertThrowMsg(this->window != nullptr, "Failed to initialize window: %s", SDL_GetError());
}

SDL_Window *SystemWindow::GetInternalWindow() {
    return this->window;
}

VkSurfaceKHR SystemWindow::CreateVulkanSurface(VkInstance instance) {
    VkSurfaceKHR surface;
    int result = SDL_Vulkan_CreateSurface(this->GetInternalWindow(), instance, &surface);

    AssertThrowMsg(result == SDL_TRUE, "Failed to create Vulkan surface: %s", SDL_GetError());

    return surface;
}

SystemWindow::~SystemWindow() {
    SDL_DestroyWindow(this->window);
}


SystemSDL::SystemSDL() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

void SystemSDL::SetCurrentWindow(SystemWindow *window) {
    this->current_window = window;
}

SystemWindow *SystemSDL::CreateSystemWindow(const char *title, int width, int height) {
    SystemWindow *window = new SystemWindow(title, width, height);
    window->Initialize();
    return window;
}

int SystemSDL::PollEvent(SystemEvent *result) {
    return SDL_PollEvent(result->GetInternalEvent());
}

SystemWindow *SystemSDL::GetCurrentWindow() {
    return this->current_window;
}

std::vector<const char *> SystemSDL::GetVulkanExtensionNames() {
    uint32_t vk_ext_count = 0;
    SDL_Window *window = this->current_window->GetInternalWindow();

    if (!SDL_Vulkan_GetInstanceExtensions(window, &vk_ext_count, nullptr)) {
        ThrowError();
    }

    std::vector<const char *> extensions(vk_ext_count);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &vk_ext_count, extensions.data())) {
        ThrowError();
    }

    return extensions;
}

SystemSDL::~SystemSDL() {
    SDL_Quit();
}

void SystemSDL::ThrowError()
{
    std::string message = SDL_GetError();

    DebugLog(LogType::Error, "SDL Error: %s", message.c_str());

    throw std::runtime_error(message);
}