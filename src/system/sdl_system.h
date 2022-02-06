//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_SDL_SYSTEM_H
#define HYPERION_SDL_SYSTEM_H

#include <SDL2/SDL.h>
#include <vector>

class SystemWindow {
public:
    SystemWindow(const char *title, int width, int height);
    SDL_Window *GetSDLWindow();
    ~SystemWindow();

private:
    SDL_Window *window;
};

class SystemSDL {
public:
    SystemSDL();
    SystemWindow CreateWindow(const char *title, int width, int height);
    void SetCurrentWindow(const SystemWindow &window);
    SystemWindow GetCurrentWindow(void);
    std::vector<const char *> GetVulkanExtensionNames();
    ~SystemSDL();
private:
    SystemWindow current_window = SystemWindow(nullptr, 0, 0);
};


#endif //HYPERION_SDL_SYSTEM_H
