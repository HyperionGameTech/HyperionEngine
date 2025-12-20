/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <input/InputManager.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Spinlock.hpp>

#include <engine/EngineGlobals.hpp>

#include <InputManager.generated.inl>

namespace hyperion {

static SlabAllocator s_inputMouseLockStateAllocator(sizeof(InputMouseLockState), alignof(InputMouseLockState), 32, AF_THREAD_SAFE);

#pragma region InputEventSink

InputEventSink::InputEventSink()
    : m_lockState(0)
{
}

InputEventSink::~InputEventSink() = default;

void InputEventSink::Push(SystemEvent&& evt)
{
    Spinlock<SPMC> spinlock(&m_lockState);
    spinlock.LockWriter();
    HYP_DEFER({ spinlock.UnlockWriter(); });

    m_events.PushBack(std::move(evt));

    m_notifier.Produce(1);
}

bool InputEventSink::Poll(SystemEvents& outEvents)
{
    if (!m_notifier.IsInSignalState())
    {
        return false;
    }

    Spinlock<SPMC> spinlock(&m_lockState);
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
        g_inputManager->RemoveMouseLockState(mouseLockState);
    }

    mouseLockState = other.mouseLockState;
    other.mouseLockState = nullptr;

    return *this;
}

InputMouseLockScope::~InputMouseLockScope()
{
    if (mouseLockState)
    {
        g_inputManager->RemoveMouseLockState(mouseLockState);
    }
}

void InputMouseLockScope::Reset()
{
    if (mouseLockState)
    {
        g_inputManager->RemoveMouseLockState(mouseLockState);

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
    SetIsMouseLocked(false);

    for (int i = 0; i < int(m_mouseLockStates.Size()); i++)
    {
        InputMouseLockState* state = m_mouseLockStates[i];
        auto it = m_mouseLockStates.Begin() + i;

        if (m_mouseLockStates.IndexOf(it) == i)
        {
            state->~InputMouseLockState();
            s_inputMouseLockStateAllocator.Free(state);
        }
    }
}

void InputManager::CheckEvent(SystemEvent* event)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    switch (event->GetType())
    {
    case SystemEvent::KEYDOWN:
        KeyDown(event->GetKeyCode());

        break;
    case SystemEvent::KEYUP:
        KeyUp(event->GetKeyCode());

        break;
    case SystemEvent::MOUSEBUTTON_DOWN:
        for (Bitset::BitIndex index : Bitset(event->GetMouseButtons()))
        {
            MouseButtonDown(MouseButtonKey(index));
        }

        break;
    case SystemEvent::MOUSEBUTTON_UP:
        for (Bitset::BitIndex index : Bitset(event->GetMouseButtons()))
        {
            MouseButtonUp(MouseButtonKey(index));
        }

        break;
    case SystemEvent::MOUSEMOTION:
        UpdateMousePosition();

        break;
    case SystemEvent::WINDOW_RESIZED:
        UpdateWindowSize(event->GetWindowResizeDimensions());

        break;
    default:
        return;
    }
}

bool InputManager::IsMouseLocked() const
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    return m_isMouseLocked;
}

void InputManager::PushMouseLockState(bool mouseLocked)
{
    InputMouseLockState* mouseLockState = (InputMouseLockState*)s_inputMouseLockStateAllocator.Allocate();

    new (mouseLockState) InputMouseLockState;
    mouseLockState->locked = mouseLocked;

    ApplyMouseLockState(mouseLockState);
}

void InputManager::PopMouseLockState()
{
    Mutex::Guard guard(m_mouseLockStatesMutex);

    if (m_mouseLockStates.Empty())
    {
        // Default state if none active
        SetIsMouseLocked(false);

        return;
    }

    InputMouseLockState* lastState = m_mouseLockStates.PopBack();
    SetIsMouseLocked(lastState->locked);

    if (!m_mouseLockStates.Contains(lastState))
    {
        lastState->~InputMouseLockState();
        s_inputMouseLockStateAllocator.Free(lastState);
    }
}

InputMouseLockScope InputManager::AcquireMouseLock()
{
    InputMouseLockState* mouseLockState = (InputMouseLockState*)s_inputMouseLockStateAllocator.Allocate();

    new (mouseLockState) InputMouseLockState;
    mouseLockState->locked = true;

    ApplyMouseLockState(mouseLockState);

    return InputMouseLockScope { mouseLockState };
}

void InputManager::SetIsMouseLocked(bool isMouseLocked)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

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
    AssertOnThread(g_gameThread);

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
    AssertOnThread(g_gameThread);

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
    AssertOnThread(g_gameThread);

    if (!m_window)
    {
        return;
    }

    if (m_windowSize == newSize)
    {
        return;
    }

    m_windowSize = newSize;
}

void InputManager::SetKey(KeyCode key, bool pressed)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        m_inputState.keyStates.Set(uint32(key), pressed);
    }
}

void InputManager::SetMouseButton(MouseButtonKey btn, bool pressed)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

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
    AssertOnThread(g_gameThread);

    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        return m_inputState.keyStates.Test(uint32(key));
    }

    return false;
}

bool InputManager::IsButtonDown(MouseButtonKey btn) const
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        return m_inputState.mouseButtonStates & MouseButtonState(1u << uint32(btn));
    }

    return false;
}

EnumFlags<MouseButtonState> InputManager::GetButtonStates() const
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

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

void InputManager::ApplyMouseLockState(InputMouseLockState* mouseLockState)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread); /// \todo Move to main thread

    Mutex::Guard guard(m_mouseLockStatesMutex);

    if (!mouseLockState)
    {
        if (m_mouseLockStates.Empty())
        {
            // apply default state
            SetIsMouseLocked(false);
        }

        return;
    }

    //SizeType currentIndex = m_mouseLockStates.IndexOf(mouseLockState);

    //if (currentIndex != -1)
    //{
    //    if (currentIndex == m_mouseLockStates.Size() - 1)
    //    {
    //        return; // already active
    //    }

    //    std::swap(m_mouseLockStates[currentIndex], m_mouseLockStates[m_mouseLockStates.Size() - 1]);

    //    SetIsMouseLocked(mouseLockState->locked);

    //    return;
    //}

    m_mouseLockStates.PushBack(mouseLockState);

    SetIsMouseLocked(mouseLockState->locked);
}

void InputManager::RemoveMouseLockState(InputMouseLockState* mouseLockState)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread); /// \todo Move to main thread

    if (!mouseLockState)
    {
        return;
    }

    Mutex::Guard guard(m_mouseLockStatesMutex);

    auto it = m_mouseLockStates.Find(mouseLockState);
    Assert(it != m_mouseLockStates.End());

    auto eraseIt = m_mouseLockStates.Erase(it);

    if (!m_mouseLockStates.Contains(mouseLockState))
    {
        mouseLockState->~InputMouseLockState();
        s_inputMouseLockStateAllocator.Free(mouseLockState);
    }

    if (eraseIt == m_mouseLockStates.End()) // was it at the end?
    {
        SetIsMouseLocked(m_mouseLockStates.Any() ? m_mouseLockStates.Back()->locked : false);
    }
}

#pragma endregion InputManager

} // namespace hyperion
