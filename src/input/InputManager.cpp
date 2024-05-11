/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <input/InputManager.hpp>

#include <core/system/AppContext.hpp>
#include <core/system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>

namespace hyperion {

InputManager::InputManager()
    : m_window(nullptr)
{
}

InputManager::~InputManager()
{
}

void InputManager::CheckEvent(SystemEvent *event)
{
    Threads::AssertOnThread(ThreadName::THREAD_INPUT);

    switch (event->GetType()) {
    case SystemEventType::EVENT_KEYDOWN:
        KeyDown(event->GetNormalizedKeyCode());
        break;
    case SystemEventType::EVENT_KEYUP:
        KeyUp(event->GetNormalizedKeyCode());
        break;
    case SystemEventType::EVENT_MOUSEBUTTON_DOWN:
        for (Pair<Bitset::BitIndex, bool> it : Bitset(event->GetMouseButtons())) {
            if (it.second) {
                MouseButtonDown(MouseButton(it.first));
            }
        }
        
        break;
    case SystemEventType::EVENT_MOUSEBUTTON_UP:
        for (Pair<Bitset::BitIndex, bool> it : Bitset(event->GetMouseButtons())) {
            if (it.second) {
                MouseButtonUp(MouseButton(it.first));
            }
        }

        break;
    case SystemEventType::EVENT_MOUSEMOTION:
        UpdateMousePosition();
        break;
    case SystemEventType::EVENT_WINDOW_EVENT:
    {
        const auto window_event_type = event->GetWindowEventType();

        switch (window_event_type) {
        case SystemWindowEventType::EVENT_WINDOW_RESIZED:
            UpdateWindowSize();
            break;
        }
    }
    default:
        return;
    }
}

void InputManager::SetMousePosition(Vec2i position)
{
    if (!m_window) {
        return;
    }

    m_previous_mouse_position = m_mouse_position;
    m_mouse_position = position;
    
    m_window->SetMousePosition(position);
}

void InputManager::UpdateMousePosition()
{
    Threads::AssertOnThread(ThreadName::THREAD_INPUT);
    
    if (!m_window) {
        return;
    }

    m_previous_mouse_position = m_mouse_position;
    m_mouse_position = m_window->GetMousePosition();
}

void InputManager::UpdateWindowSize()
{
    //Threads::AssertOnThread(ThreadName::THREAD_INPUT);
    
    if (!m_window) {
        return;
    }

    const Vec2u window_size = m_window->GetDimensions();

    m_window_size.x.store(window_size.x, std::memory_order_relaxed);
    m_window_size.y.store(window_size.y, std::memory_order_relaxed);
}

void InputManager::SetKey(int key, bool pressed)
{
    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        m_input_state.last_key_states[key] = pressed;
    }
}

void InputManager::SetMouseButton(MouseButton btn, bool pressed)
{
    if (int(btn) >= 0 && int(btn) < NUM_MOUSE_BUTTONS) {
        m_input_state.mouse_button_states[int(btn)] = pressed;
    }
}

bool InputManager::IsKeyDown(int key) const
{
    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        return m_input_state.key_states[key];
    }

    return false;
}

bool InputManager::IsButtonDown(MouseButton btn) const
{
    if (int(btn) >= 0 && int(btn) < NUM_MOUSE_BUTTONS) {
        return m_input_state.mouse_button_states[int(btn)];
    }

    return false;
}

EnumFlags<MouseButtonState> InputManager::GetButtonStates() const
{
    EnumFlags<MouseButtonState> state = MouseButtonState::NONE;

    for (uint i = 0; i < NUM_MOUSE_BUTTONS; i++) {
        if (m_input_state.mouse_button_states[i]) {
            state |= MouseButtonState(1u << i);
        }
    }

    return state;
}

} // namespace hyperion
