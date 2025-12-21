/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include <core/Defines.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/containers/Bitset.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/containers/Bitset.hpp>

#include <core/math/Vector2.hpp>

#include <core/threading/Semaphore.hpp>

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
    Bitset keyStates;
    EnumFlags<MouseButtonState> mouseButtonStates;

    InputState()
        : keyStates {},
          mouseButtonStates {}
    {
    }
};

class InputEventNotifier final : public Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE, threading::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>>
{
};

using SystemEvents = Array<SystemEvent, DynamicAllocator>;

HYP_CLASS()
class InputManager : public ObjectBase
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

    HYP_METHOD()
    HYP_API bool IsMouseLocked() const;

    HYP_METHOD()
    HYP_API void PushMouseLockState(bool mouseLocked);

    HYP_METHOD()
    HYP_API void PopMouseLockState();

    HYP_API InputMouseLockScope AcquireMouseLock();

    HYP_METHOD()
    const Vec2i& GetMousePosition() const
    {
        Mutex::Guard guard(m_snapshotMtx);
        return m_pFrontBuffer->m_mousePosition;
    }

    HYP_METHOD()
    HYP_API void SetMousePosition(Vec2i position);

    HYP_FORCE_INLINE const Vec2i& GetPreviousMousePosition() const
    {
        Mutex::Guard guard(m_snapshotMtx);
        return m_pFrontBuffer->m_previousMousePosition;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Vec2i& GetWindowSize() const
    {
        Mutex::Guard guard(m_snapshotMtx);
        return m_pFrontBuffer->m_windowSize;
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
    HYP_API bool IsButtonDown(MouseButtonKey btn) const;

    HYP_API EnumFlags<MouseButtonState> GetButtonStates() const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsButtonUp(MouseButtonKey btn) const
    {
        return !IsButtonDown(btn);
    }

    HYP_FORCE_INLINE ApplicationWindow* GetWindow() const
    {
        return m_window;
    }

    HYP_FORCE_INLINE void SetWindow(ApplicationWindow* window)
    {
        if (m_window == window)
        {
            return;
        }

        m_window = window;
    }

    void ProcessEvent(SystemEvent* event);

    void MainThreadUpdate();
    void BufferSwap();

    bool PollEvent(SystemEvent& outEvent);

private:
    void SetIsMouseLocked(bool isMouseLocked);

    void UpdateMousePosition();
    void UpdateWindowSize(Vec2i newSize);

    void SetKey(KeyCode key, bool pressed);
    void SetMouseButton(MouseButtonKey btn, bool pressed);

    void ApplyMouseLockState(InputMouseLockState* mouseLockState);
    void RemoveMouseLockState(InputMouseLockState* mouseLockState);

    struct Snapshot
    {
        SystemEvents eventQueue;
        InputState m_inputState;
        Vec2i m_mousePosition;
        Vec2i m_previousMousePosition;
        Vec2i m_windowSize;
        bool m_isMouseLocked;
    } m_snapshots[2];

    Snapshot* m_pFrontBuffer;
    Snapshot* m_pBackBuffer;

    uint32 m_frontBufferOffset;

    mutable Mutex m_snapshotMtx;

    Array<InputMouseLockState*> m_mouseLockStates;
    Mutex m_mouseLockStatesMutex;

    ApplicationWindow* m_window;
};

} // namespace hyperion
