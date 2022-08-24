#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include <core/lib/FlatMap.hpp>

#include "system/SdlSystem.hpp"

#include <functional>
#include <mutex>
#include <cstring>
#include <atomic>

namespace hyperion {

struct AtomicScalar2D {
    std::atomic_int x { 0 };
    std::atomic_int y { 0 };
};

struct InputState {
    struct KeyState {
        std::atomic_bool is_pressed{false};
    };

    struct MouseButtonState {
        std::atomic_bool is_pressed{false};
    };

    KeyState key_states[NUM_KEYBOARD_KEYS];
    MouseButtonState mouse_button_states[NUM_MOUSE_BUTTONS];
};

class InputManager {
public:
    InputManager(SystemWindow *window);
    ~InputManager();

    void CheckEvent(SystemEvent *event);

    AtomicScalar2D &GetMousePosition()             { return m_mouse_position; }
    const AtomicScalar2D &GetMousePosition() const { return m_mouse_position; }

    AtomicScalar2D &GetWindowSize()                { return m_window_size; }
    const AtomicScalar2D &GetWindowSize() const    { return m_window_size; }

    void SetMousePosition(int x,  int y)          { GetWindow()->SetMousePosition(x, y); }

    void KeyDown(int key)                         { SetKey(key, true); }
    void KeyUp(int key)                           { SetKey(key, false); }

    void MouseButtonDown(int btn)                 { SetMouseButton(btn, true); }
    void MouseButtonUp(int btn)                   { SetMouseButton(btn, false); }

    void UpdateMousePosition();
    void UpdateWindowSize();

    bool IsKeyDown(int key) const;
    bool IsKeyUp(int key) const                   { return !IsKeyDown(key); }
    bool IsButtonDown(int btn) const;  
    bool IsButtonUp(int btn) const                { return !IsButtonDown(btn); }

    void SetWindow(SystemWindow *_window)         { this->window = _window; };
    SystemWindow *GetWindow()                     { return this->window; };

private:
    InputState     m_input_state;
    AtomicScalar2D m_mouse_position;
    AtomicScalar2D m_window_size;

    SystemWindow *window = nullptr;

    std::mutex key_mutex;

    void SetKey(int key, bool pressed);
    void SetMouseButton(int btn, bool pressed);
};

} // namespace hyperion

#endif
