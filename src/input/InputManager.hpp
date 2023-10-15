#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include <core/lib/FlatMap.hpp>
#include <math/Vector2.hpp>

#include "system/Application.hpp"

#include <functional>
#include <mutex>
#include <cstring>
#include <atomic>

namespace hyperion {

template <bool IsAtomic>
struct Scalar2D { };

template<>
struct Scalar2D<true>
{
    std::atomic<Int> x { 0 };
    std::atomic<Int> y { 0 };

    Int GetX() const
        { return x.load(std::memory_order_relaxed); }

    Int GetY() const
        { return y.load(std::memory_order_relaxed); }

    explicit operator Vector2() const
        { return Vector2(Float(GetX()), Float(GetY())); }
};

template<>
struct Scalar2D<false>
{
    Int x { 0 };
    Int y { 0 };

    Scalar2D() = default;

    Scalar2D(Int x, Int y)
    {
        Scalar2D::x = x;
        Scalar2D::y = y;
    }

    Scalar2D(const Scalar2D &other) = default;
    Scalar2D &operator=(const Scalar2D &other) = default;
    Scalar2D(Scalar2D &&other) noexcept = default;
    Scalar2D &operator=(Scalar2D &other) noexcept = default;

    Int GetX() const
        { return x; }

    Int GetY() const
        { return y; }

    explicit operator Vector2() const
        { return Vector2(Float(GetX()), Float(GetY())); }
};

struct InputState
{
    struct KeyState
    {
        std::atomic_bool is_pressed{false};
    };

    struct MouseButtonState
    {
        std::atomic_bool is_pressed{false};
    };

    KeyState key_states[NUM_KEYBOARD_KEYS];
    KeyState last_key_states[NUM_KEYBOARD_KEYS];
    MouseButtonState mouse_button_states[NUM_MOUSE_BUTTONS];
};

class InputManager
{
public:
    InputManager();
    ~InputManager();

    void CheckEvent(SystemEvent *event);

    const Scalar2D<true> &GetMousePosition() const
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
    bool IsKeyStateChanged(int key, bool *previous_key_state);
    bool IsButtonDown(int btn) const;  
    bool IsButtonUp(int btn) const { return !IsButtonDown(btn); }

    void SetWindow(ApplicationWindow *window)
        { m_window = window; }

    ApplicationWindow *GetWindow()
        { return m_window; }

private:
    InputState m_input_state;
    Scalar2D<true> m_mouse_position;
    Scalar2D<true> m_window_size;

    ApplicationWindow *m_window;

    std::mutex key_mutex;

    void SetKey(int key, bool pressed);
    void SetMouseButton(int btn, bool pressed);
};

} // namespace hyperion

#endif
