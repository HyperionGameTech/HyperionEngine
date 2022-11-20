#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include <core/lib/FlatMap.hpp>
#include <math/Vector2.hpp>

#include "system/SdlSystem.hpp"

#include <functional>
#include <mutex>
#include <cstring>
#include <atomic>

namespace hyperion {

template <bool IsAtomic>
struct Scalar2D
{
    std::conditional_t<IsAtomic, std::atomic<Int>, Int> x { 0 };
    std::conditional_t<IsAtomic, std::atomic<Int>, Int> y { 0 };
};

template <bool IsAtomic>
struct MousePosition { };

template<>
struct MousePosition<true> : Scalar2D<true>
{
    Int GetX() const
        { return x.load(std::memory_order_relaxed); }

    Int GetY() const
        { return y.load(std::memory_order_relaxed); }

    explicit operator Vector2() const
        { return Vector2(Float(GetX()), Float(GetY())); }
};

template<>
struct MousePosition<false> : Scalar2D<false>
{
    MousePosition() = default;

    MousePosition(Int x, Int y)
    {
        Scalar2D::x = x;
        Scalar2D::y = y;
    }

    MousePosition(const MousePosition &other) = default;
    MousePosition &operator=(const MousePosition &other) = default;
    MousePosition(MousePosition &&other) noexcept = default;
    MousePosition &operator=(MousePosition &other) noexcept = default;

    Int GetX() const
        { return x; }

    Int GetY() const
        { return y; }

    explicit operator Vector2() const
        { return Vector2(Float(GetX()), Float(GetY())); }
};

struct InputState
{
    struct KeyState {
        std::atomic_bool is_pressed{false};
    };

    struct MouseButtonState {
        std::atomic_bool is_pressed{false};
    };

    KeyState key_states[NUM_KEYBOARD_KEYS];
    MouseButtonState mouse_button_states[NUM_MOUSE_BUTTONS];
};

class InputManager
{
public:
    InputManager();
    ~InputManager();

    void CheckEvent(SystemEvent *event);

    const MousePosition<true> &GetMousePosition() const
        { return m_mouse_position; }

    Scalar2D<true> &GetWindowSize()
        { return m_window_size; }

    const Scalar2D<true> &GetWindowSize() const
        { return m_window_size; }

    void SetMousePosition(Int x, Int y);

    void KeyDown(int key)
        { SetKey(key, true); }

    void KeyUp(int key)
        { SetKey(key, false); }

    void MouseButtonDown(int btn)
        { SetMouseButton(btn, true); }

    void MouseButtonUp(int btn)
        { SetMouseButton(btn, false); }

    void UpdateMousePosition();
    void UpdateWindowSize();

    bool IsKeyDown(int key) const;
    bool IsKeyUp(int key) const { return !IsKeyDown(key); }
    bool IsButtonDown(int btn) const;  
    bool IsButtonUp(int btn) const { return !IsButtonDown(btn); }

    void SetWindow(ApplicationWindow *window)
        { m_window = window; }

    ApplicationWindow *GetWindow()
        { return m_window; }

private:
    InputState m_input_state;
    MousePosition<true> m_mouse_position;
    Scalar2D<true> m_window_size;

    ApplicationWindow *m_window;

    std::mutex key_mutex;

    void SetKey(int key, bool pressed);
    void SetMouseButton(int btn, bool pressed);
};

} // namespace hyperion

#endif
