/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Input/Keyboard.hpp>
#include <Input/Mouse.hpp>
#include <Input/InputConstants.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/BitField.hpp>

#include <Core/Memory/Pimpl.hpp>

namespace Hyperion {

struct InputState;
struct TouchEvent;
struct ControllerAnalogData;
enum class ControllerButton : uint16;

HYP_CLASS(Abstract)
class ENGINE_API InputHandlerBase : public ObjectBase
{
    HYP_OBJECT_BODY(InputHandlerBase);

public:
    InputHandlerBase();
    
    InputHandlerBase(const InputHandlerBase& other) = delete;
    InputHandlerBase& operator=(const InputHandlerBase& other) = delete;

    InputHandlerBase(InputHandlerBase&& other) noexcept = delete;
    InputHandlerBase& operator=(InputHandlerBase&& other) noexcept = delete;

    virtual ~InputHandlerBase();

    HYP_FORCE_INLINE const BitField<NumKeyboardKeys>& GetKeyStates() const
    {
        return m_keyStates;
    }

    HYP_FORCE_INLINE EnumFlags<MouseButtonState> GetMouseButtonStates() const
    {
        return m_mouseButtonStates;
    }

    HYP_FORCE_INLINE void SetDeltaTime(double deltaTime)
    {
        m_deltaTime = deltaTime;
    }

    bool IsKeyDown(KeyCode key) const;
    bool IsKeyUp(KeyCode key) const;

    bool IsMouseButtonDown(MouseButtonKey btn) const;
    bool IsMouseButtonUp(MouseButtonKey btn) const;

    /*! \brief Get the current touch movement delta (joystick input)
     *  \return Vec2f where x = strafe (left/right), y = forward/back, range -1 to 1 */
    HYP_FORCE_INLINE const Vec2f& GetTouchMovementDelta() const
    {
        return m_touchMovementDelta;
    }

    /*! \brief Set the touch movement delta from joystick input
     *  \param delta Vec2f where x = strafe, y = forward/back */
    HYP_FORCE_INLINE void SetTouchMovementDelta(const Vec2f& delta)
    {
        m_touchMovementDelta = delta;
    }

    virtual bool OnKeyDown(const KeyboardEvent& evt);
    virtual bool OnKeyUp(const KeyboardEvent& evt);

    virtual bool OnMouseDown(const MouseEvent& evt);
    virtual bool OnMouseUp(const MouseEvent& evt);

    virtual bool OnTouchDown(const TouchEvent& evt)
    {
        return false;
    }

    virtual bool OnTouchUp(const TouchEvent& evt)
    {
        return false;
    }

    virtual bool OnTouchMove(const TouchEvent& evt)
    {
        return false;
    }

    virtual bool OnMouseMove(const MouseEvent& evt)
    {
        return false;
    }

    virtual bool OnMouseDrag(const MouseEvent& evt)
    {
        return false;
    }

    virtual bool OnMouseLeave(const MouseEvent& evt)
    {
        return false;
    }

    virtual bool OnClick(const MouseEvent& evt)
    {
        return false;
    }

    virtual bool OnGainFocus(const MouseEvent& evt)
    {
        return false;
    }

    virtual bool OnLoseFocus(const MouseEvent& evt)
    {
        return false;
    }

    virtual bool OnControllerButtonDown(ControllerButton btn)
    {
        return false;
    }

    virtual bool OnControllerButtonUp(ControllerButton btn)
    {
        return false;
    }

    virtual bool OnControllerAnalogMove(const ControllerAnalogData& data);

    HYP_FORCE_INLINE const Vec2f& GetControllerMoveDelta() const
    {
        return m_controllerMoveDelta;
    }

    HYP_FORCE_INLINE void SetControllerMoveDelta(const Vec2f& delta)
    {
        m_controllerMoveDelta = delta;
    }

    HYP_FORCE_INLINE const Vec2f& GetControllerLookDelta() const
    {
        return m_controllerLookDelta;
    }

    HYP_FORCE_INLINE void SetControllerLookDelta(const Vec2f& delta)
    {
        m_controllerLookDelta = delta;
    }

private:
    BitField<NumKeyboardKeys> m_keyStates;
    EnumFlags<MouseButtonState> m_mouseButtonStates;

    Vec2f m_touchMovementDelta;
    Vec2f m_controllerMoveDelta;
    Vec2f m_controllerLookDelta;

protected:
    double m_deltaTime;
};

HYP_CLASS()
class NullInputHandler final : public InputHandlerBase
{
    HYP_OBJECT_BODY(NullInputHandler);

public:
    NullInputHandler() = default;
    virtual ~NullInputHandler() override = default;
};

} // namespace Hyperion
