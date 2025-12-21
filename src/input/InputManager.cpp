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

constexpr bool UseSharedBuffer = false; // use to reduce contention but may cause out-of-sync between events and global state

#pragma region InputMouseLockScope

InputMouseLockScope& InputMouseLockScope::operator=(InputMouseLockScope&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (mouseLockState && inputMgr)
    {
        inputMgr->RemoveMouseLockState(mouseLockState);
    }

    mouseLockState = other.mouseLockState;
    other.mouseLockState = nullptr;

    return *this;
}

InputMouseLockScope::~InputMouseLockScope()
{
    if (mouseLockState && inputMgr)
    {
        inputMgr->RemoveMouseLockState(mouseLockState);
    }
}

void InputMouseLockScope::Reset()
{
    if (mouseLockState && inputMgr)
    {
        inputMgr->RemoveMouseLockState(mouseLockState);

        mouseLockState = nullptr;
    }
}

#pragma endregion InputMouseLockScope

#pragma region InputManager

InputManager::InputManager()
    : m_window(nullptr),
      m_pFrontBuffer(nullptr),
      m_pBackBuffer(nullptr)
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

bool InputManager::IsMouseLocked() const
{
    Mutex::Guard guard(m_snapshotMtx);
    return m_pBackBuffer->m_isMouseLocked;
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

    Mutex::Guard guard(m_snapshotMtx);

    if (m_pBackBuffer->m_isMouseLocked == isMouseLocked)
    {
        return; // already set
    }

    if (m_window)
    {
        m_window->SetIsMouseLocked(isMouseLocked);
    }
    else
    {
        // set to false if no window
        isMouseLocked = false;
    }

    m_pBackBuffer->m_isMouseLocked = isMouseLocked;
}

void InputManager::SetMousePosition(Vec2i position)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    if (!m_window)
    {
        return;
    }

    Mutex::Guard guard(m_snapshotMtx);

    m_pBackBuffer->m_mousePosition = position;

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

    Mutex::Guard guard(m_snapshotMtx);

    m_pBackBuffer->m_previousMousePosition = m_pBackBuffer->m_mousePosition;
    m_pBackBuffer->m_mousePosition = m_window->GetMousePosition();

    if (m_pBackBuffer->m_isMouseLocked && m_window != nullptr)
    {
        m_window->SetMousePosition(m_pBackBuffer->m_previousMousePosition);
        m_pBackBuffer->m_mousePosition = m_window->GetMousePosition();
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

    Mutex::Guard guard(m_snapshotMtx);

    if (m_pBackBuffer->m_windowSize == newSize)
    {
        return;
    }

    m_pBackBuffer->m_windowSize = newSize;
}

void InputManager::SetKey(KeyCode key, bool pressed)
{
    AssertOnThread(g_mainThread);

    Mutex::Guard guard(m_snapshotMtx);

    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        m_pBackBuffer->m_inputState.keyStates.Set(uint32(key), pressed);
    }
}

void InputManager::SetMouseButton(MouseButtonKey btn, bool pressed)
{
    AssertOnThread(g_mainThread);

    Mutex::Guard guard(m_snapshotMtx);

    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        if (pressed)
        {
            m_pBackBuffer->m_inputState.mouseButtonStates |= MouseButtonState(1u << uint32(btn));
        }
        else
        {
            m_pBackBuffer->m_inputState.mouseButtonStates &= MouseButtonState(~(1u << uint32(btn)));
        }
    }
}

bool InputManager::IsKeyDown(KeyCode key) const
{
    Mutex::Guard guard(m_snapshotMtx);

    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        return m_pFrontBuffer->m_inputState.keyStates.Test(uint32(key));
    }

    return false;
}

bool InputManager::IsButtonDown(MouseButtonKey btn) const
{
    Mutex::Guard guard(m_snapshotMtx);

    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        return m_pFrontBuffer->m_inputState.mouseButtonStates & MouseButtonState(1u << uint32(btn));
    }

    return false;
}

EnumFlags<MouseButtonState> InputManager::GetButtonStates() const
{
    Mutex::Guard guard(m_snapshotMtx);

    EnumFlags<MouseButtonState> state = MouseButtonState::NONE;

    for (uint32 i = 0; i < NUM_MOUSE_BUTTONS; i++)
    {
        if (m_pFrontBuffer->m_inputState.mouseButtonStates & MouseButtonState(1u << i))
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

void InputManager::ProcessEvent(SystemEvent* event)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    Mutex::Guard guard(m_snapshotMtx);

    switch (event->GetType())
    {
    case SystemEvent::KEYDOWN:
        SetKey(event->GetKeyCode(), true);

        break;
    case SystemEvent::KEYUP:
        SetKey(event->GetKeyCode(), false);

        break;
    case SystemEvent::MOUSEBUTTON_DOWN:
        for (Bitset::BitIndex index : Bitset(event->GetMouseButtons()))
        {
            SetMouseButton(MouseButtonKey(index), true);
        }

        break;
    case SystemEvent::MOUSEBUTTON_UP:
        for (Bitset::BitIndex index : Bitset(event->GetMouseButtons()))
        {
            SetMouseButton(MouseButtonKey(index), false);
        }

        break;
    case SystemEvent::MOUSEMOTION:
        UpdateMousePosition();

        break;
    case SystemEvent::WINDOW_RESIZED:
        UpdateWindowSize(event->GetWindowResizeDimensions());

        break;
    default:
        break;
    }

    m_pBackBuffer->eventQueue.PushBack(std::move(*event));
}

void InputManager::BufferSwap()
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    Mutex::Guard guard(m_snapshotMtx);

    m_pFrontBuffer->eventQueue.Clear();
    m_frontBufferOffset = 0;

    std::swap(m_pFrontBuffer, m_pBackBuffer);
}

bool InputManager::PollEvent(SystemEvent& outEvent)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (m_frontBufferOffset >= m_pFrontBuffer->eventQueue.Size())
    {
        return false;
    }

    outEvent = std::move(m_pFrontBuffer->eventQueue[m_frontBufferOffset++]);
}

void InputManager::MainThreadUpdate()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    Mutex::Guard guard(m_mouseLockStatesMutex);
    SetIsMouseLocked(m_mouseLockStates.Any() ? m_mouseLockStates.Back()->locked : false);
}

#pragma endregion InputManager

} // namespace hyperion
