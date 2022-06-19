#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include <core/lib/flat_map.h>

#include "system/sdl_system.h"

#include <functional>
#include <mutex>
#include <cstring>

namespace hyperion {

struct MousePosition {
    std::atomic_int mx{0};
    std::atomic_int my{0};
};

struct InputState {
    struct KeyState {
        std::atomic_bool is_pressed{false};
    };

    struct MouseButtonState {
        std::atomic_bool is_pressed{false};
    };

    KeyState         key_states[NUM_KEYBOARD_KEYS];
    MouseButtonState mouse_button_states[NUM_MOUSE_BUTTONS];
};

class InputManager {
public:
    InputManager(SystemWindow *window);
    ~InputManager();

    void CheckEvent(SystemEvent *event);

    MousePosition &GetMousePosition()             { return m_mouse_position; }
    const MousePosition &GetMousePosition() const { return m_mouse_position; }

    void SetMousePosition(int x,  int y)          { GetWindow()->SetMousePosition(x, y); }

    void KeyDown(int key)                         { SetKey(key, true); }
    void KeyUp(int key)                           { SetKey(key, false); }

    void MouseButtonDown(int btn)                 { SetMouseButton(btn, true); }
    void MouseButtonUp(int btn)                   { SetMouseButton(btn, false); }

    void UpdateMousePosition();

    bool IsKeyDown(int key) const;
    bool IsKeyUp(int key) const                   { return !IsKeyDown(key); }
    bool IsButtonDown(int btn) const;  
    bool IsButtonUp(int btn) const                { return !IsButtonDown(btn); }

    void SetWindow(SystemWindow *_window)         { this->window = _window; };
    SystemWindow *GetWindow()                     { return this->window; };

private:
    InputState    m_input_state;
    MousePosition m_mouse_position;

    SystemWindow *window = nullptr;

    std::mutex key_mutex;

    void SetKey(int key, bool pressed);
    void SetMouseButton(int btn, bool pressed);
};

} // namespace hyperion

#endif
