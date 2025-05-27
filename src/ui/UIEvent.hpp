/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_EVENT_HPP
#define HYPERION_UI_EVENT_HPP

#include <ui/UIObject.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

namespace hyperion {

struct MouseEvent;
struct KeyboardEvent;

class IUIEvent
{
public:
    virtual ~IUIEvent() = default;

    virtual InputManager* GetInputManager() = 0;

    virtual MouseEvent* GetMouseEvent() = 0;
    virtual KeyboardEvent* GetKeyboardEvent() = 0;
};

class UIMouseEvent : public IUIEvent
{
public:
    UIMouseEvent(const MouseEvent& mouse_event)
        : m_mouse_event(mouse_event)
    {
    }

    virtual ~UIMouseEvent() override = default;

    virtual InputManager* GetInputManager() override
    {
        return m_mouse_event.input_manager;
    }

    virtual MouseEvent* GetMouseEvent() override
    {
        return &m_mouse_event;
    }

    virtual KeyboardEvent* GetKeyboardEvent() override
    {
        return nullptr;
    }

private:
    MouseEvent m_mouse_event;
};

class UIKeyboardEvent : public IUIEvent
{
public:
    UIKeyboardEvent(const KeyboardEvent& keyboard_event)
        : m_keyboard_event(keyboard_event)
    {
    }

    virtual ~UIKeyboardEvent() override = default;

    virtual InputManager* GetInputManager() override
    {
        return m_keyboard_event.input_manager;
    }

    virtual MouseEvent* GetMouseEvent() override
    {
        return nullptr;
    }

    virtual KeyboardEvent* GetKeyboardEvent() override
    {
        return &m_keyboard_event;
    }

private:
    KeyboardEvent m_keyboard_event;
};

} // namespace hyperion

#endif