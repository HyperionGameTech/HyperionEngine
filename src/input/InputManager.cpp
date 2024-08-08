/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <input/InputManager.hpp>

#include <core/HypClassUtils.hpp>

#include <core/system/AppContext.hpp>
#include <core/system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(InputManager);

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
        for (Bitset::BitIndex index : Bitset(event->GetMouseButtons())) {
            MouseButtonDown(MouseButton(index));
        }
        
        break;
    case SystemEventType::EVENT_MOUSEBUTTON_UP:
        for (Bitset::BitIndex index : Bitset(event->GetMouseButtons())) {
            MouseButtonUp(MouseButton(index));
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
            UpdateWindowSize(event->GetWindowResizeDimensions());
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

void InputManager::UpdateWindowSize(Vec2i new_size)
{
    //Threads::AssertOnThread(ThreadName::THREAD_INPUT);
    
    if (!m_window) {
        return;
    }

    m_window->HandleResize(new_size);

    m_window_size.x.store(new_size.x, std::memory_order_relaxed);
    m_window_size.y.store(new_size.y, std::memory_order_relaxed);
}

void InputManager::SetKey(KeyCode key, bool pressed)
{
    if (uint32(key) < NUM_KEYBOARD_KEYS) {
        m_input_state.key_states[uint32(key)] = pressed;
    }
}

void InputManager::SetMouseButton(MouseButton btn, bool pressed)
{
    if (uint32(btn) < NUM_MOUSE_BUTTONS) {
        m_input_state.mouse_button_states[uint32(btn)] = pressed;
    }
}

bool InputManager::IsKeyDown(KeyCode key) const
{
    if (uint32(key) < NUM_KEYBOARD_KEYS) {
        return m_input_state.key_states[uint32(key)];
    }

    return false;
}

bool InputManager::IsButtonDown(MouseButton btn) const
{
    if (uint32(btn) < NUM_MOUSE_BUTTONS) {
        return m_input_state.mouse_button_states[uint32(btn)];
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
