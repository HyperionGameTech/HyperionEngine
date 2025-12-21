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

class HYP_API InputEventSink
{
public:
    InputEventSink();
    InputEventSink(const InputEventSink& other) = delete;
    InputEventSink& operator=(const InputEventSink& other) = delete;
    InputEventSink(InputEventSink&& other) noexcept = delete;
    InputEventSink& operator=(InputEventSink&& other) noexcept = delete;
    ~InputEventSink();

    void Push(SystemEvent&& evt);
    bool Poll(SystemEvents& outEvents);

private:
    InputEventNotifier m_notifier;
    SystemEvents m_events;
    volatile int64 m_lockState;
};

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

    HYP_API void CheckEvent(SystemEvent* event);

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
        return GetBufferedData()->m_mousePosition;
    }

    HYP_METHOD()
    HYP_API void SetMousePosition(Vec2i position);

    HYP_FORCE_INLINE const Vec2i& GetPreviousMousePosition() const
    {
        return GetBufferedData()->m_previousMousePosition;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Vec2i& GetWindowSize() const
    {
        return GetBufferedData()->m_windowSize;
    }

    void KeyDown(KeyCode key)
    {
        SetKey(key, true);
    }

    void KeyUp(KeyCode key)
    {
        SetKey(key, false);
    }

    void MouseButtonDown(MouseButtonKey btn)
    {
        SetMouseButton(btn, true);
    }

    void MouseButtonUp(MouseButtonKey btn)
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

    void MainThreadUpdate();
    void GameThreadSync();

private:
    void SetIsMouseLocked(bool isMouseLocked);

    void UpdateMousePosition();
    void UpdateWindowSize(Vec2i newSize);

    void SetKey(KeyCode key, bool pressed);
    void SetMouseButton(MouseButtonKey btn, bool pressed);

    void ApplyMouseLockState(InputMouseLockState* mouseLockState);
    void RemoveMouseLockState(InputMouseLockState* mouseLockState);

    struct BufferedData
    {
        enum
        {
            PRODUCER,
            CONSUMER,
            SHARED
        };

        InputState m_inputState;
        Vec2i m_mousePosition;
        Vec2i m_previousMousePosition;
        Vec2i m_windowSize;
        bool m_isMouseLocked;
    } m_bufferedData[3]; // 0 = main thread, 1 = game thread, 2 = shared copy

    class BufferedDataLockScope
    {
    public:
        BufferedDataLockScope(BufferedData* pBufferedData, volatile int32* pLockState)
            : m_pBufferedData(pBufferedData),
              m_pLockState(pLockState)
        {
        }

        BufferedDataLockScope(const BufferedDataLockScope& other) = delete;
        BufferedDataLockScope& operator=(const BufferedDataLockScope& other) = delete;

        BufferedDataLockScope(BufferedDataLockScope&& other) noexcept
            : m_pBufferedData(other.m_pBufferedData),
              m_pLockState(other.m_pLockState)
        {
            other.m_pBufferedData = nullptr;
            other.m_pLockState = nullptr;
        }

        BufferedDataLockScope& operator=(BufferedDataLockScope&& other) noexcept = delete;

        ~BufferedDataLockScope()
        {
            if (m_pLockState)
                AtomicDecrement(m_pLockState);
        }

        BufferedData* operator->()
        {
            return m_pBufferedData;
        }

        const BufferedData* operator->() const
        {
            return m_pBufferedData;
        }

    private:
        BufferedData* m_pBufferedData;
        volatile int32* m_pLockState;
    };

    volatile int32 m_lockState;

    BufferedDataLockScope GetBufferedData();

    HYP_FORCE_INLINE const BufferedDataLockScope GetBufferedData() const
    {
        return const_cast<InputManager*>(this)->GetBufferedData();
    }

    Array<InputMouseLockState*> m_mouseLockStates;
    Mutex m_mouseLockStatesMutex;

    ApplicationWindow* m_window;
};

} // namespace hyperion
