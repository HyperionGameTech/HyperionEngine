/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <input/InputHandler.hpp>
#include <input/InputManager.hpp>

#include <core/utilities/ByteUtil.hpp>

namespace hyperion {

#pragma region InputHandlerBase

InputHandlerBase::InputHandlerBase()
    : m_inputState(MakePimpl<InputState>()),
      m_deltaTime(0.016667)
{
}

InputHandlerBase::~InputHandlerBase()
{
}

const Bitset& InputHandlerBase::GetKeyStates() const
{
    return m_inputState->keyStates;
}

bool InputHandlerBase::IsKeyDown(KeyCode key) const
{
    return m_inputState->keyStates.Test(uint32(key));
}

bool InputHandlerBase::IsKeyUp(KeyCode key) const
{
    return !m_inputState->keyStates.Test(uint32(key));
}

bool InputHandlerBase::IsMouseButtonDown(MouseButton btn) const
{
    return m_inputState->mouseButtonStates & MouseButtonState(1u << uint32(btn));
}

bool InputHandlerBase::IsMouseButtonUp(MouseButton btn) const
{
    return !(m_inputState->mouseButtonStates & MouseButtonState(1u << uint32(btn)));
}

bool InputHandlerBase::OnKeyDown_Impl(const KeyboardEvent& evt)
{
    if (uint32(evt.keyCode) < NUM_KEYBOARD_KEYS)
    {
        m_inputState->keyStates.Set(uint32(evt.keyCode), true);
    }

    return true;
}

bool InputHandlerBase::OnKeyUp_Impl(const KeyboardEvent& evt)
{
    if (uint32(evt.keyCode) < NUM_KEYBOARD_KEYS)
    {
        m_inputState->keyStates.Set(uint32(evt.keyCode), false);
    }

    return true;
}

bool InputHandlerBase::OnMouseDown_Impl(const MouseEvent& evt)
{
    FOR_EACH_BIT(uint32(evt.mouseButtons), i)
    {
        if (i < NUM_MOUSE_BUTTONS)
        {
            m_inputState->mouseButtonStates |= (1u << i);
        }
    }

    return true;
}

bool InputHandlerBase::OnMouseUp_Impl(const MouseEvent& evt)
{
    FOR_EACH_BIT(uint32(evt.mouseButtons), i)
    {
        if (i < NUM_MOUSE_BUTTONS)
        {
            m_inputState->mouseButtonStates &= (1u << i);
        }
    }

    return true;
}

bool InputHandlerBase::OnMouseLeave_Impl(const MouseEvent& evt)
{
    return false;
}

#pragma endregion InputHandlerBase

} // namespace hyperion
