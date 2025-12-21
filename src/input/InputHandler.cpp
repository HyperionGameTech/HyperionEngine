/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <input/InputHandler.hpp>
#include <input/InputManager.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <InputHandler.generated.inl>

namespace hyperion {

#pragma region InputHandlerBase

InputHandlerBase::InputHandlerBase()
    : m_mouseButtonStates(0),
      m_deltaTime(0.016667)
{
    m_keyStates.SetNumBits(NUM_KEYBOARD_KEYS);
}

InputHandlerBase::~InputHandlerBase()
{
}

bool InputHandlerBase::IsKeyDown(KeyCode key) const
{
    return m_keyStates.Test(uint32(key));
}

bool InputHandlerBase::IsKeyUp(KeyCode key) const
{
    return !m_keyStates.Test(uint32(key));
}

bool InputHandlerBase::IsMouseButtonDown(MouseButtonKey btn) const
{
    return m_mouseButtonStates & MouseButtonState(1u << uint32(btn));
}

bool InputHandlerBase::IsMouseButtonUp(MouseButtonKey btn) const
{
    return !(m_mouseButtonStates & MouseButtonState(1u << uint32(btn)));
}

bool InputHandlerBase::OnKeyDown_Impl(const KeyboardEvent& evt)
{
    if (uint32(evt.keyCode) < NUM_KEYBOARD_KEYS)
    {
        m_keyStates.Set(uint32(evt.keyCode), true);
    }

    // default to not handled
    return false;
}

bool InputHandlerBase::OnKeyUp_Impl(const KeyboardEvent& evt)
{
    if (uint32(evt.keyCode) < NUM_KEYBOARD_KEYS)
    {
        m_keyStates.Set(uint32(evt.keyCode), false);
    }

    // default to not handled
    return false;
}

bool InputHandlerBase::OnMouseDown_Impl(const MouseEvent& evt)
{
    m_mouseButtonStates |= evt.mouseButtons;

    // default to not handled
    return false;
}

bool InputHandlerBase::OnMouseUp_Impl(const MouseEvent& evt)
{
    m_mouseButtonStates &= ~evt.mouseButtons;

    // default to not handled
    return false;
}

bool InputHandlerBase::OnMouseLeave_Impl(const MouseEvent& evt)
{
    return false;
}

#pragma endregion InputHandlerBase

} // namespace hyperion
