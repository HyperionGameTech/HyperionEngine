#include "InputManager.hpp"

#include <Threads.hpp>

#include <iostream>
#include <string.h>

namespace hyperion {

using namespace v2;

InputManager::InputManager(SystemWindow *window)
    : window(window)
{
}

InputManager::~InputManager()
{
}

void InputManager::CheckEvent(SystemEvent *event)
{
    //Threads::AssertOnThread(THREAD_INPUT);

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
        case SystemEventType::EVENT_WINDOW_EVENT:
        {
            auto window_event_type = event->GetWindowEventType();

            switch (window_event_type) {
            case SystemWindowEventType::EVENT_WINDOW_RESIZED:
                this->UpdateWindowSize();
                break;
            }
        }
        default:
            return;
    }
}

void InputManager::UpdateMousePosition()
{
    //Threads::AssertOnThread(THREAD_INPUT);

    int mx, my;

    GetWindow()->GetMouseState(&mx, &my);

    m_mouse_position.x.store(mx);
    m_mouse_position.y.store(my);
}

void InputManager::UpdateWindowSize()
{
    //Threads::AssertOnThread(THREAD_INPUT);

    int width, height;

    GetWindow()->GetSize(&width, &height);

    m_window_size.x.store(width);
    m_window_size.y.store(height);
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
