/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <input/InputManager.hpp>

#include <core/system/AppContext.hpp>

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
        MouseButtonDown(event->GetMouseButton());
        break;
    case SystemEventType::EVENT_MOUSEBUTTON_UP:
        MouseButtonUp(event->GetMouseButton());
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

void InputManager::SetMousePosition(int x, int y)
{
    if (!m_window) {
        return;
    }

    m_window->SetMousePosition(x, y);
}

void InputManager::UpdateMousePosition()
{
    //Threads::AssertOnThread(ThreadName::THREAD_INPUT);
    
    if (!m_window) {
        return;
    }

    const MouseState mouse_state = m_window->GetMouseState();

    m_mouse_position.x.store(mouse_state.x, std::memory_order_relaxed);
    m_mouse_position.y.store(mouse_state.y, std::memory_order_relaxed);
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
        const bool prev_value = m_input_state.key_states[key].is_pressed.exchange(pressed, std::memory_order_relaxed);
        m_input_state.last_key_states[key].is_pressed.store(prev_value, std::memory_order_relaxed);
    }
}

void InputManager::SetMouseButton(int btn, bool pressed)
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        m_input_state.mouse_button_states[btn].is_pressed.store(pressed, std::memory_order_relaxed);
    }
}

bool InputManager::IsKeyDown(int key) const
{
    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        return m_input_state.key_states[key].is_pressed.load(std::memory_order_relaxed);
    }

    return false;
}

bool InputManager::IsKeyStateChanged(int key, bool *previous_key_state)
{
    AssertThrow(previous_key_state != nullptr);

    const bool previous_value = *previous_key_state;

    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        bool is_pressed = m_input_state.key_states[key].is_pressed.load(std::memory_order_relaxed);

        *previous_key_state = is_pressed;

        return is_pressed != previous_value;
    } else {
        *previous_key_state = false;
    }

    return false;
}

bool InputManager::IsButtonDown(int btn) const
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        return m_input_state.mouse_button_states[btn].is_pressed.load(std::memory_order_relaxed);
    }

    return false;
}

} // namespace hyperion
