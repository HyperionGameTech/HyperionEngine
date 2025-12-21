/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include <core/Defines.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/math/Vector2.hpp>

#include <core/threading/Semaphore.hpp>

#include <input/Keyboard.hpp>
#include <input/Mouse.hpp>

namespace hyperion {

class ApplicationWindow;
class Event;

struct InputState
{
    mutable volatile int32 keyStates[NUM_KEYBOARD_KEYS / 32 + 1];
    mutable volatile int32 mouseButtonStates;

    InputState()
        : keyStates {},
          mouseButtonStates {}
    {
    }
};

class InputEventQueue;

HYP_CLASS()
class InputManager : public ObjectBase
{
    HYP_OBJECT_BODY(InputManager);

    friend struct InputMouseLockScope;

public:
    explicit InputManager(ApplicationWindow* ownerWindow);

    InputManager(const InputManager& other) = delete;
    InputManager& operator=(const InputManager& other) = delete;

    InputManager(InputManager&& other) noexcept = delete;
    InputManager& operator=(InputManager&& other) noexcept = delete;

    ~InputManager();

    HYP_METHOD()
    bool IsMouseLocked() const;

    HYP_METHOD()
    void PushMouseLockState(bool mouseLocked);

    HYP_METHOD()
    void PopMouseLockState();

    InputMouseLockScope AcquireMouseLock();

    HYP_METHOD()
    Vec2i GetMousePosition() const
    {
        return m_mousePosition;
    }

    Vec2i GetPreviousMousePosition() const
    {
        return m_previousMousePosition;
    }

    HYP_METHOD()
    Vec2i GetVirtualMousePosition() const
    {
        return m_virtualMousePosition;
    }

    HYP_METHOD()
    Vec2i GetPreviousVirtualMousePosition() const
    {
        return m_previousVirtualMousePosition;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetWindowSize() const
    {
        return m_windowSize;
    }

    HYP_METHOD()
    bool IsKeyDown(KeyCode key) const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsKeyUp(KeyCode key) const
    {
        return !IsKeyDown(key);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsShiftDown() const
    {
        return IsKeyDown(KeyCode::KEY_LSHIFT) || IsKeyDown(KeyCode::KEY_RSHIFT);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsAltDown() const
    {
        return IsKeyDown(KeyCode::KEY_LALT) || IsKeyDown(KeyCode::KEY_RALT);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsCtrlDown() const
    {
        return IsKeyDown(KeyCode::KEY_LCTRL) || IsKeyDown(KeyCode::KEY_RCTRL);
    }

    HYP_METHOD()
    bool IsButtonDown(MouseButtonKey btn) const;

   EnumFlags<MouseButtonState> GetButtonStates() const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsButtonUp(MouseButtonKey btn) const
    {
        return !IsButtonDown(btn);
    }

    HYP_FORCE_INLINE ApplicationWindow* GetWindow() const
    {
        return m_ownerWindow;
    }

    void ProcessEvent(Event&& event);

    void MainThreadUpdate();
    void BufferSwap();

    bool PollEvent(Event& outEvent);

private:
    void SetIsMouseLocked(bool isMouseLocked);

    void UpdateMousePosition(Event& event);
    void UpdateWindowSize(Vec2i newSize);

    void SetKey(KeyCode key, bool pressed);
    void SetMouseButton(MouseButtonKey btn, bool pressed);

    void ApplyMouseLockState(InputMouseLockState* mouseLockState);
    void RemoveMouseLockState(InputMouseLockState* mouseLockState);

    struct AtomicVec2i
    {
        volatile int32 x;
        volatile int32 y;

        AtomicVec2i()
            : x(0),
              y(0)
        {
        }

        AtomicVec2i(const Vec2i& vec)
            : x(vec.x),
              y(vec.y)
        {
        }

        AtomicVec2i(const AtomicVec2i& other)
            : x(AtomicAdd(const_cast<volatile int32*>(&other.x), 0)),
              y(AtomicAdd(const_cast<volatile int32*>(&other.y), 0))
        {
        }

        AtomicVec2i& operator=(const AtomicVec2i& other)
        {
            AtomicExchange(&x, AtomicAdd(const_cast<volatile int32*>(&other.x), 0));
            AtomicExchange(&y, AtomicAdd(const_cast<volatile int32*>(&other.y), 0));

            return *this;
        }

        HYP_FORCE_INLINE operator Vec2i() const
        {
            return Vec2i(
                AtomicAdd(const_cast<volatile int32*>(&x), 0),
                AtomicAdd(const_cast<volatile int32*>(&y), 0));
        }

        HYP_FORCE_INLINE AtomicVec2i& operator=(const Vec2i& vec)
        {
            AtomicExchange(&x, vec.x);
            AtomicExchange(&y, vec.y);

            return *this;
        }
    };

    InputEventQueue* m_eventQueue;
    InputState m_inputState;

    AtomicVec2i m_mousePosition;
    AtomicVec2i m_previousMousePosition;

    AtomicVec2i m_virtualMousePosition;
    AtomicVec2i m_previousVirtualMousePosition;

    AtomicVec2i m_windowSize;

    Array<InputMouseLockState*, Pool> m_mouseLockStates;
    Mutex m_mouseLockStatesMutex;

    ApplicationWindow* m_ownerWindow;

    bool m_isMouseLocked;
};

} // namespace hyperion
