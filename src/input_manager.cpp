#include "input_manager.h"
#include <iostream>
#include <cstring>

namespace hyperion {

InputEvent::InputEvent()
    : m_is_empty(true)
{
}

InputEvent::InputEvent(Callback &&handler)
    : m_handler(std::move(handler)),
      m_is_empty(false)
{
}

InputEvent::InputEvent(const InputEvent &other)
    : m_handler(other.m_handler),
      m_is_empty(other.m_is_empty)
{
}

InputManager::InputManager(SystemWindow *window)
    : window(window)
{
    key_events = new InputEvent[NUM_KEYBOARD_KEYS];
    mouse_events = new InputEvent[NUM_MOUSE_BUTTONS];

    std::memset(key_states, false, NUM_KEYBOARD_KEYS * sizeof(bool));
    std::memset(mouse_states, false, NUM_MOUSE_BUTTONS * sizeof(bool));

    mouse_x = 0.0;
    mouse_y = 0.0;
}

InputManager::~InputManager()
{
    delete[] key_events;
    delete[] mouse_events;
}

void InputManager::CheckEvent(SystemEvent *event)
{
    switch (event->GetType()) {
        case SystemEventType::EVENT_KEYDOWN:
            this->KeyDown(event->GetKeyCode());
            break;
        case SystemEventType::EVENT_KEYUP:
            this->KeyUp(event->GetKeyCode());
            break;
        case SystemEventType::EVENT_MOUSEBUTTON_DOWN:
            this->MouseButtonDown(event->GetMouseButton());
            break;
        case SystemEventType::EVENT_MOUSEBUTTON_UP:
            this->MouseButtonUp(event->GetMouseButton());
            break;
        case SystemEventType::EVENT_MOUSEMOTION:
            this->UpdateMousePosition();
            break;
        default:
            return;
    }
}

void InputManager::UpdateMousePosition()
{
    int mx, my;

    this->GetMousePosition(&mx, &my);

    mouse_x = static_cast<double>(mx);
    mouse_y = static_cast<double>(my);
}

void InputManager::SetKey(int key, bool pressed)
{
    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        /* Set all letters to uppercase */
        if (key >= 'a' && key <= 'z') {
            key = 'A' + (key - 'a');
        }

        InputEvent &handler = key_events[key];

        if (!handler.IsEmpty()) {
            handler.Trigger(window, pressed);
        }

        key_states[key] = pressed;
    }
}

void InputManager::SetMouseButton(int btn, bool pressed)
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        InputEvent &handler = mouse_events[btn];

        if (!handler.IsEmpty()) {
            handler.Trigger(window, pressed);
        }

        mouse_states[btn] = pressed;
    }
}

bool InputManager::IsKeyDown(int key) const
{
    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        return key_states[key];
    }

    return false;
}

bool InputManager::IsButtonDown(int btn) const
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        return mouse_states[btn];
    }

    return false;
}

bool InputManager::RegisterKeyEvent(int key, const InputEvent &evt)
{
    DebugLog(LogType::Debug, "Registering key event for [%d]\n", key);

    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        key_events[key] = evt;
        return true;
    }

    return false;
}

bool InputManager::RegisterClickEvent(int btn, const InputEvent &evt)
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        mouse_events[btn] = evt;
        return true;
    }

    return false;
}

} // namespace hyperion
