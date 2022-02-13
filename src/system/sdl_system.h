//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_SDL_SYSTEM_H
#define HYPERION_SDL_SYSTEM_H

#include <SDL2/SDL.h>
#include <vector>
#include <string>

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
    void Initialize();
    SDL_Window *GetSDLWindow();
    ~SystemWindow();

    const char *title;
    int width, height;
private:
    SDL_Window *window;
};

class SystemSDL {
public:
    SystemSDL();
    static SystemWindow CreateWindow(const char *title, int width, int height);
    static int          PollEvent(SystemEvent *result);
    void SetCurrentWindow(const SystemWindow &window);
    SystemWindow GetCurrentWindow();

    std::vector<const char *> GetVulkanExtensionNames();
    ~SystemSDL();
private:
    SystemWindow current_window = SystemWindow(nullptr, 0, 0);
};


#endif //HYPERION_SDL_SYSTEM_H
