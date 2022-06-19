#include "input_manager.h"
#include <iostream>
#include <string.h>

namespace hyperion {

InputManager::InputManager(SystemWindow *window)
    : window(window)
{
    mouse_x = 0.0;
    mouse_y = 0.0;
}

InputManager::~InputManager()
{
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

        m_input_state.key_states[key].is_pressed.store(pressed);
    }
}

void InputManager::SetMouseButton(int btn, bool pressed)
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        m_input_state.mouse_button_states[btn].is_pressed.store(pressed);
    }
}

bool InputManager::IsKeyDown(int key) const
{
    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        return m_input_state.key_states[key].is_pressed.load();
    }

    return false;
}

bool InputManager::IsButtonDown(int btn) const
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        return m_input_state.mouse_button_states[btn].is_pressed.load();
    }

    return false;
}

} // namespace hyperion
