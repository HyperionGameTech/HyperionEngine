/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <input/InputHandler.hpp>
#include <input/InputManager.hpp>

#include <Core/utilities/ByteUtil.hpp>

#include <InputHandler.generated.inl>

namespace Hyperion {

static constexpr double InitialDeltaTime = 1.0 / 60.0;

#pragma region InputHandlerBase

InputHandlerBase::InputHandlerBase()
    : m_mouseButtonStates(0),
      m_deltaTime(InitialDeltaTime)
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

bool InputHandlerBase::OnKeyDown(const KeyboardEvent& evt)
{
    if (uint32(evt.keyCode) < NUM_KEYBOARD_KEYS)
    {
        m_keyStates.Set(uint32(evt.keyCode), true);
    }

    // default to not handled
    return false;
}

bool InputHandlerBase::OnKeyUp(const KeyboardEvent& evt)
{
    if (uint32(evt.keyCode) < NUM_KEYBOARD_KEYS)
    {
        m_keyStates.Set(uint32(evt.keyCode), false);
    }

    // default to not handled
    return false;
}

bool InputHandlerBase::OnMouseDown(const MouseEvent& evt)
{
    m_mouseButtonStates |= evt.mouseButtons;

    // default to not handled
    return false;
}

bool InputHandlerBase::OnMouseUp(const MouseEvent& evt)
{
    m_mouseButtonStates &= ~evt.mouseButtons;

    // default to not handled
    return false;
}

#pragma endregion InputHandlerBase

} // namespace Hyperion
