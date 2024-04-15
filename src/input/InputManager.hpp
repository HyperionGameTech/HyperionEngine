/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include <core/lib/FlatMap.hpp>
#include <math/Vector2.hpp>

#include <system/Application.hpp>

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
    std::atomic<int> x { 0 };
    std::atomic<int> y { 0 };

    int GetX() const
        { return x.load(std::memory_order_relaxed); }

    int GetY() const
        { return y.load(std::memory_order_relaxed); }

    explicit operator Vector2() const
        { return Vector2(float(GetX()), float(GetY())); }
};

template<>
struct Scalar2D<false>
{
    int x { 0 };
    int y { 0 };

    Scalar2D() = default;

    Scalar2D(int x, int y)
    {
        Scalar2D::x = x;
        Scalar2D::y = y;
    }

    Scalar2D(const Scalar2D &other) = default;
    Scalar2D &operator=(const Scalar2D &other) = default;
    Scalar2D(Scalar2D &&other) noexcept = default;
    Scalar2D &operator=(Scalar2D &other) noexcept = default;

    int GetX() const
        { return x; }

    int GetY() const
        { return y; }

    explicit operator Vector2() const
        { return Vector2(float(GetX()), float(GetY())); }
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

    KeyState            key_states[NUM_KEYBOARD_KEYS];
    KeyState            last_key_states[NUM_KEYBOARD_KEYS];
    MouseButtonState    mouse_button_states[NUM_MOUSE_BUTTONS];
};

class InputManager
{
public:
    HYP_API InputManager();
    InputManager(const InputManager &other)                 = delete;
    InputManager &operator=(const InputManager &other)      = delete;
    InputManager(InputManager &&other) noexcept             = delete;
    InputManager &operator=(InputManager &&other) noexcept  = delete;
    HYP_API ~InputManager();

    HYP_API void CheckEvent(SystemEvent *event);

    const Scalar2D<true> &GetMousePosition() const
        { return m_mouse_position; }

    Scalar2D<true> &GetWindowSize()
        { return m_window_size; }

    const Scalar2D<true> &GetWindowSize() const
        { return m_window_size; }

    HYP_API void SetMousePosition(int x, int y);

    void KeyDown(int key)
        { SetKey(key, true); }

    void KeyUp(int key)
        { SetKey(key, false); }

    void MouseButtonDown(int btn)
        { SetMouseButton(btn, true); }

    void MouseButtonUp(int btn)
        { SetMouseButton(btn, false); }

    HYP_API void UpdateMousePosition();
    HYP_API void UpdateWindowSize();

    HYP_API bool IsKeyStateChanged(int key, bool *previous_key_state);
    HYP_API bool IsKeyDown(int key) const;
    bool IsKeyUp(int key) const { return !IsKeyDown(key); }

    HYP_API bool IsButtonDown(int btn) const;  
    bool IsButtonUp(int btn) const { return !IsButtonDown(btn); }

    ApplicationWindow *GetWindow() const
        { return m_window; }

    void SetWindow(ApplicationWindow *window)
        { m_window = window; }

private:
    InputState          m_input_state;
    Scalar2D<true>      m_mouse_position;
    Scalar2D<true>      m_window_size;

    ApplicationWindow   *m_window;

    HYP_API void SetKey(int key, bool pressed);
    HYP_API void SetMouseButton(int btn, bool pressed);
};

} // namespace hyperion

#endif
