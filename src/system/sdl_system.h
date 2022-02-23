//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_SDL_SYSTEM_H
#define HYPERION_SDL_SYSTEM_H

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL.h>
#include <vector>
#include <string>

// replace with file from util.h
template <class T>
struct non_owning_ptr {
    T *ptr = nullptr;

    T *operator->() { return ptr; }
    const T *operator->() const { return ptr; }
};

enum SystemEventType {
    EVENT_KEYDOWN = SDL_KEYDOWN,
    EVENT_SHUTDOWN = SDL_QUIT,
    EVENT_KEYUP    = SDL_KEYUP,
    EVENT_MOUSEMOTION    = SDL_MOUSEMOTION,
    EVENT_MOUSEBUTTON_DOWN = SDL_MOUSEBUTTONDOWN,
    EVENT_MOUSEBUTTON_UP   = SDL_MOUSEBUTTONUP,
};

class SystemEvent {
public:
    SystemEventType GetType();
    SDL_Event *GetInternalEvent();
private:
    SDL_Event sdl_event;
};

class SystemWindow {
public:
    SystemWindow(const char *_title, int _width, int _height);
    SystemWindow(const SystemWindow &other) = delete;
    SystemWindow &operator=(const SystemWindow &other) = delete;
    void Initialize();
    SDL_Window *GetInternalWindow();

    VkSurfaceKHR CreateVulkanSurface(VkInstance instance);
    ~SystemWindow();

    const char *title;
    int width, height;
private:
    SDL_Window *window;
};

class SystemSDL {
public:
    SystemSDL();
    static SystemWindow *CreateSystemWindow(const char *title, int width, int height);
    static int          PollEvent(SystemEvent *result);
    void SetCurrentWindow(SystemWindow *window);
    SystemWindow *GetCurrentWindow();

    std::vector<const char *> GetVulkanExtensionNames();
    ~SystemSDL();

    static void ThrowError();
private:
    SystemWindow *current_window;
};


#endif //HYPERION_SDL_SYSTEM_H
