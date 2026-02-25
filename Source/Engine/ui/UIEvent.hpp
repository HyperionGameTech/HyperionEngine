/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <ui/UIObject.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

namespace Hyperion {

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
    UIMouseEvent(const MouseEvent& mouseEvent)
        : m_mouseEvent(mouseEvent)
    {
    }

    virtual ~UIMouseEvent() override = default;

    virtual MouseEvent* GetMouseEvent() override
    {
        return &m_mouseEvent;
    }

    virtual KeyboardEvent* GetKeyboardEvent() override
    {
        return nullptr;
    }

private:
    MouseEvent m_mouseEvent;
};

class UIKeyboardEvent : public IUIEvent
{
public:
    UIKeyboardEvent(const KeyboardEvent& keyboardEvent)
        : m_keyboardEvent(keyboardEvent)
    {
    }

    virtual ~UIKeyboardEvent() override = default;

    virtual InputManager* GetInputManager() override
    {
        return m_keyboardEvent.inputManager;
    }

    virtual MouseEvent* GetMouseEvent() override
    {
        return nullptr;
    }

    virtual KeyboardEvent* GetKeyboardEvent() override
    {
        return &m_keyboardEvent;
    }

private:
    KeyboardEvent m_keyboardEvent;
};

} // namespace Hyperion
