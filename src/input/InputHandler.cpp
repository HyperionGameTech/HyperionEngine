/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <input/InputHandler.hpp>
#include <input/InputManager.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <InputHandler.generated.inl>

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

bool InputHandlerBase::IsMouseButtonDown(MouseButtonKey btn) const
{
    return m_inputState->mouseButtonStates & MouseButtonState(1u << uint32(btn));
}

bool InputHandlerBase::IsMouseButtonUp(MouseButtonKey btn) const
{
    return !(m_inputState->mouseButtonStates & MouseButtonState(1u << uint32(btn)));
}

bool InputHandlerBase::OnKeyDown_Impl(const KeyboardEvent& evt)
{
    if (uint32(evt.keyCode) < NUM_KEYBOARD_KEYS)
    {
        m_inputState->keyStates.Set(uint32(evt.keyCode), true);
    }

    // default to not handled
    return false;
}

bool InputHandlerBase::OnKeyUp_Impl(const KeyboardEvent& evt)
{
    if (uint32(evt.keyCode) < NUM_KEYBOARD_KEYS)
    {
        m_inputState->keyStates.Set(uint32(evt.keyCode), false);
    }

    // default to not handled
    return false;
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

    // default to not handled
    return false;
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

    // default to not handled
    return false;
}

bool InputHandlerBase::OnMouseLeave_Impl(const MouseEvent& evt)
{
    return false;
}

#pragma endregion InputHandlerBase

} // namespace hyperion
