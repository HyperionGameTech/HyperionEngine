/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <input/InputManager.hpp>

#include <core/object/HypClassUtils.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Spinlock.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

#pragma region InputEventSink

InputEventSink::InputEventSink()
    : m_lock_state(0)
{
}

InputEventSink::~InputEventSink() = default;

void InputEventSink::Push(SystemEvent&& evt)
{
    Spinlock spinlock(m_lock_state);
    spinlock.LockWriter();

    m_events.PushBack(std::move(evt));

    m_notifier.Produce(1);

    spinlock.UnlockWriter();
}

bool InputEventSink::Poll(Array<SystemEvent>& out_events)
{
    if (!m_notifier.IsInSignalState())
    {
        return false;
    }

    Spinlock spinlock(m_lock_state);
    spinlock.LockReader();

    if (m_events.Empty())
    {
        spinlock.UnlockReader();
        return false;
    }

    out_events = std::move(m_events);

    m_notifier.Release(out_events.Size());

    spinlock.UnlockReader();

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

    if (mouse_lock_state)
    {
        mouse_lock_state->input_manager->RemoveMouseLockState(mouse_lock_state);
    }

    mouse_lock_state = other.mouse_lock_state;
    other.mouse_lock_state = nullptr;

    return *this;
}

InputMouseLockScope::~InputMouseLockScope()
{
    if (mouse_lock_state)
    {
        mouse_lock_state->input_manager->RemoveMouseLockState(mouse_lock_state);
    }
}

void InputMouseLockScope::Reset()
{
    if (mouse_lock_state)
    {
        mouse_lock_state->input_manager->RemoveMouseLockState(mouse_lock_state);

        mouse_lock_state = nullptr;
    }
}

#pragma endregion InputMouseLockScope

#pragma region InputManager

InputManager::InputManager()
    : m_window(nullptr),
      m_is_mouse_locked(false)
{
}

InputManager::~InputManager()
{
}

void InputManager::CheckEvent(SystemEvent* event)
{
    Threads::AssertOnThread(g_game_thread);

    switch (event->GetType())
    {
    case SystemEventType::EVENT_KEYDOWN:
        HYP_LOG(Input, Debug, "Key down: {}", uint32(event->GetKeyCode()));
        KeyDown(event->GetNormalizedKeyCode());
        break;
    case SystemEventType::EVENT_KEYUP:
        KeyUp(event->GetNormalizedKeyCode());
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
    case SystemEventType::EVENT_WINDOW_EVENT:
    {
        const auto window_event_type = event->GetWindowEventType();

        switch (window_event_type)
        {
        case SystemWindowEventType::EVENT_WINDOW_RESIZED:
            UpdateWindowSize(event->GetWindowResizeDimensions());
            break;
        }
    }
    default:
        return;
    }
}

bool InputManager::IsMouseLocked() const
{
    return m_is_mouse_locked;
}

void InputManager::PushMouseLockState(bool mouse_locked)
{
    Mutex::Guard guard(m_mouse_lock_states_mutex);

    InputMouseLockState& mouse_lock_state = m_mouse_lock_states.PushBack(InputMouseLockState {
        this,
        mouse_locked });

    ApplyMouseLockState(&mouse_lock_state);
}

void InputManager::PopMouseLockState()
{
    Mutex::Guard guard(m_mouse_lock_states_mutex);

    if (m_mouse_lock_states.Empty())
    {
        return;
    }

    m_mouse_lock_states.PopBack();

    ApplyMouseLockState(m_mouse_lock_states.Any() ? &m_mouse_lock_states.Back() : nullptr);
}

InputMouseLockScope InputManager::AcquireMouseLock()
{
    Mutex::Guard guard(m_mouse_lock_states_mutex);

    InputMouseLockState& mouse_lock_state = m_mouse_lock_states.PushBack(InputMouseLockState {
        this,
        true });

    ApplyMouseLockState(&mouse_lock_state);

    return InputMouseLockScope { mouse_lock_state };
}

void InputManager::SetIsMouseLocked(bool is_mouse_locked)
{
    if (m_is_mouse_locked == is_mouse_locked)
    {
        return;
    }

    if (is_mouse_locked)
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

    m_is_mouse_locked = is_mouse_locked;
}

void InputManager::SetMousePosition(Vec2i position)
{
    if (!m_window)
    {
        return;
    }

    m_previous_mouse_position = m_mouse_position;
    m_mouse_position = position;

    m_window->SetMousePosition(position);
}

void InputManager::UpdateMousePosition()
{
    Threads::AssertOnThread(g_game_thread);

    if (!m_window)
    {
        return;
    }

    m_previous_mouse_position = m_mouse_position;
    m_mouse_position = m_window->GetMousePosition();
}

void InputManager::UpdateWindowSize(Vec2i new_size)
{
    if (!m_window)
    {
        return;
    }

    if (m_window_size == new_size)
    {
        return;
    }

    m_window->HandleResize(new_size);

    m_window_size = new_size;
}

void InputManager::SetKey(KeyCode key, bool pressed)
{
    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        m_input_state.key_states[uint32(key)] = pressed;
    }
}

void InputManager::SetMouseButton(MouseButton btn, bool pressed)
{
    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        m_input_state.mouse_button_states[uint32(btn)] = pressed;
    }
}

bool InputManager::IsKeyDown(KeyCode key) const
{
    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        return m_input_state.key_states[uint32(key)];
    }

    return false;
}

bool InputManager::IsButtonDown(MouseButton btn) const
{
    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        return m_input_state.mouse_button_states[uint32(btn)];
    }

    return false;
}

EnumFlags<MouseButtonState> InputManager::GetButtonStates() const
{
    EnumFlags<MouseButtonState> state = MouseButtonState::NONE;

    for (uint32 i = 0; i < NUM_MOUSE_BUTTONS; i++)
    {
        if (m_input_state.mouse_button_states[i])
        {
            state |= MouseButtonState(1u << i);
        }
    }

    return state;
}

void InputManager::ApplyMouseLockState(const InputMouseLockState* mouse_lock_state)
{
    if (!mouse_lock_state)
    {
        // apply default state

        SetIsMouseLocked(false);

        return;
    }

    SetIsMouseLocked(mouse_lock_state->locked);
}

void InputManager::RemoveMouseLockState(const InputMouseLockState* mouse_lock_state)
{
    if (!mouse_lock_state)
    {
        return;
    }

    Mutex::Guard guard(m_mouse_lock_states_mutex);

    auto it = m_mouse_lock_states.Find(*mouse_lock_state);
    AssertThrow(it != m_mouse_lock_states.End());

    auto erase_it = m_mouse_lock_states.Erase(it);

    if (erase_it == m_mouse_lock_states.End())
    {
        // Update mouse lock state since last was removed
        ApplyMouseLockState(m_mouse_lock_states.Any() ? &m_mouse_lock_states.Back() : nullptr);
    }
}

#pragma endregion InputManager

} // namespace hyperion
