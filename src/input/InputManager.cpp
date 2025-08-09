/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <input/InputManager.hpp>

#include <core/object/HypClassUtils.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Spinlock.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

#pragma region InputEventSink

InputEventSink::InputEventSink()
    : m_lockState(0)
{
}

InputEventSink::~InputEventSink() = default;

void InputEventSink::Push(SystemEvent&& evt)
{
    Spinlock spinlock(m_lockState);
    spinlock.LockWriter();
    HYP_DEFER({ spinlock.UnlockWriter(); });

    m_events.PushBack(std::move(evt));

    m_notifier.Produce(1);
}

bool InputEventSink::Poll(Array<SystemEvent>& outEvents)
{
    if (!m_notifier.IsInSignalState())
    {
        return false;
    }

    Spinlock spinlock(m_lockState);
    spinlock.LockReader();
    HYP_DEFER({ spinlock.UnlockReader(); });

    if (m_events.Empty())
    {
        return false;
    }

    outEvents = std::move(m_events);

    m_notifier.Release(outEvents.Size());

    return true;
}

#pragma endregion InputEventSink

#pragma region InputMouseLockScope

InputMouseLockScope& InputMouseLockScope::operator=(InputMouseLockScope&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (mouseLockState)
    {
        mouseLockState->inputManager->RemoveMouseLockState(mouseLockState);
    }

    mouseLockState = other.mouseLockState;
    other.mouseLockState = nullptr;

    return *this;
}

InputMouseLockScope::~InputMouseLockScope()
{
    if (mouseLockState)
    {
        mouseLockState->inputManager->RemoveMouseLockState(mouseLockState);
    }
}

void InputMouseLockScope::Reset()
{
    if (mouseLockState)
    {
        mouseLockState->inputManager->RemoveMouseLockState(mouseLockState);

        mouseLockState = nullptr;
    }
}

#pragma endregion InputMouseLockScope

#pragma region InputManager

InputManager::InputManager()
    : m_window(nullptr),
      m_isMouseLocked(false)
{
}

InputManager::~InputManager()
{
}

void InputManager::CheckEvent(SystemEvent* event)
{
    Threads::AssertOnThread(g_gameThread);

    switch (event->GetType())
    {
    case SystemEventType::EVENT_KEYDOWN:
        KeyDown(event->GetKeyCode());

        break;
    case SystemEventType::EVENT_KEYUP:
        KeyUp(event->GetKeyCode());

        break;
    case SystemEventType::EVENT_MOUSEBUTTON_DOWN:
        for (Bitset::BitIndex index : Bitset(event->GetMouseButtons()))
        {
            MouseButtonDown(MouseButton(index));
        }

        break;
    case SystemEventType::EVENT_MOUSEBUTTON_UP:
        for (Bitset::BitIndex index : Bitset(event->GetMouseButtons()))
        {
            MouseButtonUp(MouseButton(index));
        }

        break;
    case SystemEventType::EVENT_MOUSEMOTION:
        UpdateMousePosition();

        break;
    case SystemEventType::EVENT_WINDOW_RESIZED:
        UpdateWindowSize(event->GetWindowResizeDimensions());

        break;
    default:
        return;
    }
}

bool InputManager::IsMouseLocked() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    return m_isMouseLocked;
}

void InputManager::PushMouseLockState(bool mouseLocked)
{
    Mutex::Guard guard(m_mouseLockStatesMutex);

    InputMouseLockState& mouseLockState = m_mouseLockStates.PushBack(InputMouseLockState {
        this,
        mouseLocked });

    ApplyMouseLockState(&mouseLockState);
}

void InputManager::PopMouseLockState()
{
    Mutex::Guard guard(m_mouseLockStatesMutex);

    if (m_mouseLockStates.Empty())
    {
        return;
    }

    m_mouseLockStates.PopBack();

    ApplyMouseLockState(m_mouseLockStates.Any() ? &m_mouseLockStates.Back() : nullptr);
}

InputMouseLockScope InputManager::AcquireMouseLock()
{
    Mutex::Guard guard(m_mouseLockStatesMutex);

    InputMouseLockState& mouseLockState = m_mouseLockStates.PushBack(InputMouseLockState { this, true });

    ApplyMouseLockState(&mouseLockState);

    return InputMouseLockScope { mouseLockState };
}

void InputManager::SetIsMouseLocked(bool isMouseLocked)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (m_isMouseLocked == isMouseLocked)
    {
        return;
    }

    if (isMouseLocked)
    {
        if (m_window)
        {
            m_window->SetIsMouseLocked(true);
        }
    }
    else
    {
        if (m_window)
        {
            m_window->SetIsMouseLocked(false);
        }
    }

    m_isMouseLocked = isMouseLocked;
}

void InputManager::SetMousePosition(Vec2i position)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!m_window)
    {
        return;
    }

    m_previousMousePosition = m_mousePosition;
    m_mousePosition = position;

    m_window->SetMousePosition(position);
}

void InputManager::UpdateMousePosition()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!m_window)
    {
        return;
    }

    m_previousMousePosition = m_mousePosition;
    m_mousePosition = m_window->GetMousePosition();
}

void InputManager::UpdateWindowSize(Vec2i newSize)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!m_window)
    {
        return;
    }

    if (m_windowSize == newSize)
    {
        return;
    }

    m_window->HandleResize(newSize);

    m_windowSize = newSize;
}

void InputManager::SetKey(KeyCode key, bool pressed)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        m_inputState.keyStates.Set(uint32(key), pressed);
    }
}

void InputManager::SetMouseButton(MouseButton btn, bool pressed)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        if (pressed)
        {
            m_inputState.mouseButtonStates |= MouseButtonState(1u << uint32(btn));
        }
        else
        {
            m_inputState.mouseButtonStates &= MouseButtonState(~(1u << uint32(btn)));
        }
    }
}

bool InputManager::IsKeyDown(KeyCode key) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        return m_inputState.keyStates.Test(uint32(key));
    }

    return false;
}

bool InputManager::IsButtonDown(MouseButton btn) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        return m_inputState.mouseButtonStates & MouseButtonState(1u << uint32(btn));
    }

    return false;
}

EnumFlags<MouseButtonState> InputManager::GetButtonStates() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    EnumFlags<MouseButtonState> state = MouseButtonState::NONE;

    for (uint32 i = 0; i < NUM_MOUSE_BUTTONS; i++)
    {
        if (m_inputState.mouseButtonStates & MouseButtonState(1u << i))
        {
            state |= MouseButtonState(1u << i);
        }
    }

    return state;
}

void InputManager::ApplyMouseLockState(const InputMouseLockState* mouseLockState)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!mouseLockState)
    {
        // apply default state

        SetIsMouseLocked(false);

        return;
    }

    SetIsMouseLocked(mouseLockState->locked);
}

void InputManager::RemoveMouseLockState(const InputMouseLockState* mouseLockState)
{
    if (!mouseLockState)
    {
        return;
    }

    Mutex::Guard guard(m_mouseLockStatesMutex);

    auto it = m_mouseLockStates.Find(*mouseLockState);
    Assert(it != m_mouseLockStates.End());

    auto eraseIt = m_mouseLockStates.Erase(it);

    if (eraseIt == m_mouseLockStates.End())
    {
        // Update mouse lock state since last was removed
        ApplyMouseLockState(m_mouseLockStates.Any() ? &m_mouseLockStates.Back() : nullptr);
    }
}

#pragma endregion InputManager

} // namespace hyperion
