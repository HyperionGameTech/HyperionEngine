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
      m_bufferedData(),
      m_lockState(0)
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

InputManager::BufferedDataLockScope InputManager::GetBufferedData()
{
    const bool isMainThread = IsOnThread(g_mainThread);
    const int idx = int(isMainThread ? BufferedData::PRODUCER : BufferedData::CONSUMER);

#ifdef HYP_DEBUG_MODE
    if (!isMainThread)
    {
        AssertOnThread(g_gameThread);
    }
#endif

    // 0 = main thread
    // 1 = game thread copy
    return { &m_bufferedData[idx], nullptr };
}

void InputManager::CheckEvent(SystemEvent* event)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

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
    return GetBufferedData()->m_isMouseLocked;
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
        if (IsOnThread(g_mainThread))
        {
            // Default state if none active
            SetIsMouseLocked(false);
        }

        return;
    }

    InputMouseLockState* lastState = m_mouseLockStates.PopBack();

    if (IsOnThread(g_mainThread))
    {
        SetIsMouseLocked(lastState->locked);
    }

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
    AssertOnThread(g_mainThread);

    BufferedData& bd = m_bufferedData[BufferedData::PRODUCER];

    if (bd.m_isMouseLocked == isMouseLocked)
    {
        return; // already set
    }

    if (m_window)
    {
        //m_window->SetIsMouseLocked(isMouseLocked);
    }
    else
    {
        // set to false if no window
        isMouseLocked = false;
    }

    bd.m_isMouseLocked = isMouseLocked;
}

void InputManager::SetMousePosition(Vec2i position)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    if (!m_window)
    {
        return;
    }

    BufferedData& bd = m_bufferedData[BufferedData::PRODUCER];

    bd.m_mousePosition = position;

    m_window->SetMousePosition(position);
}

void InputManager::UpdateMousePosition()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    if (!m_window)
    {
        return;
    }

    BufferedData& bd = m_bufferedData[BufferedData::PRODUCER];

    bd.m_previousMousePosition = bd.m_mousePosition;
    bd.m_mousePosition = m_window->GetMousePosition();

    if (bd.m_isMouseLocked && m_window != nullptr)
    {
        m_window->SetMousePosition(bd.m_previousMousePosition);
        bd.m_mousePosition = m_window->GetMousePosition();
    }
}

void InputManager::UpdateWindowSize(Vec2i newSize)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    if (!m_window)
    {
        return;
    }

    BufferedData& bd = m_bufferedData[BufferedData::PRODUCER];

    if (bd.m_windowSize == newSize)
    {
        return;
    }

    bd.m_windowSize = newSize;
}

void InputManager::SetKey(KeyCode key, bool pressed)
{
    AssertOnThread(g_mainThread);

    BufferedData& bd = m_bufferedData[BufferedData::PRODUCER];

    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        bd.m_inputState.keyStates.Set(uint32(key), pressed);
    }
}

void InputManager::SetMouseButton(MouseButtonKey btn, bool pressed)
{
    AssertOnThread(g_mainThread);

    BufferedData& bd = m_bufferedData[BufferedData::PRODUCER];

    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        if (pressed)
        {
            bd.m_inputState.mouseButtonStates |= MouseButtonState(1u << uint32(btn));
        }
        else
        {
            bd.m_inputState.mouseButtonStates &= MouseButtonState(~(1u << uint32(btn)));
        }
    }
}

bool InputManager::IsKeyDown(KeyCode key) const
{
    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        return GetBufferedData()->m_inputState.keyStates.Test(uint32(key));
    }

    return false;
}

bool InputManager::IsButtonDown(MouseButtonKey btn) const
{
    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        return GetBufferedData()->m_inputState.mouseButtonStates & MouseButtonState(1u << uint32(btn));
    }

    return false;
}

EnumFlags<MouseButtonState> InputManager::GetButtonStates() const
{
    EnumFlags<MouseButtonState> state = MouseButtonState::NONE;

    const BufferedDataLockScope bd = GetBufferedData();

    for (uint32 i = 0; i < NUM_MOUSE_BUTTONS; i++)
    {
        if (bd->m_inputState.mouseButtonStates & MouseButtonState(1u << i))
        {
            state |= MouseButtonState(1u << i);
        }
    }

    return state;
}

void InputManager::ApplyMouseLockState(InputMouseLockState* mouseLockState)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mouseLockStatesMutex);

    if (!mouseLockState)
    {
        if (m_mouseLockStates.Empty() && IsOnThread(g_mainThread))
        {
            // apply default state
            SetIsMouseLocked(false);
        }

        return;
    }

    m_mouseLockStates.PushBack(mouseLockState);

    if (IsOnThread(g_mainThread))
    {
        SetIsMouseLocked(mouseLockState->locked);
    }
}

void InputManager::RemoveMouseLockState(InputMouseLockState* mouseLockState)
{
    HYP_SCOPE;

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

    if (eraseIt == m_mouseLockStates.End() && IsOnThread(g_mainThread)) // was it at the end?
    {
        SetIsMouseLocked(m_mouseLockStates.Any() ? m_mouseLockStates.Back()->locked : false);
    }
}

void InputManager::GameThreadSync()
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    constexpr int MaxSpins = 32;
    int numSpins = 0;

    do
    {
        int32 currentValue = 0;
        if (AtomicCompareExchange(&m_lockState, currentValue, 1))
        {
            m_bufferedData[BufferedData::CONSUMER] = m_bufferedData[BufferedData::SHARED];

            AtomicDecrement(&m_lockState);

            return;
        }
    }
    while (numSpins++ < MaxSpins);
}

void InputManager::MainThreadUpdate()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    SetIsMouseLocked(m_mouseLockStates.Any() ? m_mouseLockStates.Back()->locked : false);

    constexpr int MaxSpins = 32;
    int numSpins = 0;

    do
    {
        int32 currentValue = 0;
        if (AtomicCompareExchange(&m_lockState, currentValue, 1))
        {
            m_bufferedData[BufferedData::SHARED] = m_bufferedData[BufferedData::PRODUCER];

            AtomicDecrement(&m_lockState);

            return;
        }
    }
    while (numSpins++ < MaxSpins);
}

#pragma endregion InputManager

} // namespace hyperion
