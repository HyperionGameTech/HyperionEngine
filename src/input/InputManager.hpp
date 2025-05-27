/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef INPUT_MANAGER_HPP
#define INPUT_MANAGER_HPP

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/containers/Bitset.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/math/Vector2.hpp>

#include <input/Keyboard.hpp>
#include <input/Mouse.hpp>

namespace hyperion {

namespace sys {
class ApplicationWindow;
class SystemEvent;
} // namespace sys

using sys::ApplicationWindow;
using sys::SystemEvent;

struct InputState
{
    bool key_states[NUM_KEYBOARD_KEYS];
    bool mouse_button_states[int(MouseButton::MAX)];

    InputState()
        : key_states { false },
          mouse_button_states { false }
    {
    }
};

HYP_CLASS()

class InputManager : public HypObject<InputManager>
{
    HYP_OBJECT_BODY(InputManager);

    friend struct InputMouseLockScope;

public:
    HYP_API InputManager();
    InputManager(const InputManager& other) = delete;
    InputManager& operator=(const InputManager& other) = delete;
    InputManager(InputManager&& other) noexcept = delete;
    InputManager& operator=(InputManager&& other) noexcept = delete;
    HYP_API ~InputManager();

    HYP_API void CheckEvent(SystemEvent* event);

    HYP_METHOD()
    HYP_API bool IsMouseLocked() const;

    HYP_METHOD()
    HYP_API void PushMouseLockState(bool mouse_locked);

    HYP_METHOD()
    HYP_API void PopMouseLockState();

    HYP_API InputMouseLockScope AcquireMouseLock();

    HYP_METHOD()

    const Vec2i& GetMousePosition() const
    {
        return m_mouse_position;
    }

    HYP_METHOD()
    HYP_API void SetMousePosition(Vec2i position);

    const Vec2i& GetPreviousMousePosition() const
    {
        return m_previous_mouse_position;
    }

    HYP_METHOD()

    const Vec2i& GetWindowSize() const
    {
        return m_window_size;
    }

    void KeyDown(KeyCode key)
    {
        SetKey(key, true);
    }

    void KeyUp(KeyCode key)
    {
        SetKey(key, false);
    }

    void MouseButtonDown(MouseButton btn)
    {
        SetMouseButton(btn, true);
    }

    void MouseButtonUp(MouseButton btn)
    {
        SetMouseButton(btn, false);
    }

    HYP_METHOD()
    HYP_API bool IsKeyDown(KeyCode key) const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsKeyUp(KeyCode key) const
    {
        return !IsKeyDown(key);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsShiftDown() const
    {
        return IsKeyDown(KeyCode::LEFT_SHIFT) || IsKeyDown(KeyCode::RIGHT_SHIFT);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsAltDown() const
    {
        return IsKeyDown(KeyCode::LEFT_ALT) || IsKeyDown(KeyCode::RIGHT_ALT);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsCtrlDown() const
    {
        return IsKeyDown(KeyCode::LEFT_CTRL) || IsKeyDown(KeyCode::RIGHT_CTRL);
    }

    HYP_METHOD()
    HYP_API bool IsButtonDown(MouseButton btn) const;

    HYP_API EnumFlags<MouseButtonState> GetButtonStates() const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsButtonUp(MouseButton btn) const
    {
        return !IsButtonDown(btn);
    }

    HYP_FORCE_INLINE ApplicationWindow* GetWindow() const
    {
        return m_window;
    }

    HYP_FORCE_INLINE void SetWindow(ApplicationWindow* window)
    {
        m_window = window;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode();
    }

private:
    void SetIsMouseLocked(bool is_mouse_locked);

    void UpdateMousePosition();
    void UpdateWindowSize(Vec2i new_size);

    void SetKey(KeyCode key, bool pressed);
    void SetMouseButton(MouseButton btn, bool pressed);

    void ApplyMouseLockState(const InputMouseLockState* mouse_lock_state);
    void RemoveMouseLockState(const InputMouseLockState* mouse_lock_state);

    InputState m_input_state;
    Vec2i m_mouse_position;
    Vec2i m_previous_mouse_position;
    Vec2i m_window_size;
    bool m_is_mouse_locked;

    LinkedList<InputMouseLockState> m_mouse_lock_states;
    Mutex m_mouse_lock_states_mutex;

    ApplicationWindow* m_window;
};

} // namespace hyperion

#endif
