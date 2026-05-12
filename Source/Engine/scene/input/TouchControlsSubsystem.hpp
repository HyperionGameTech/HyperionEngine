/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/math/Vector2.hpp>
#include <Core/math/Color.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/Map.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <scene/Subsystem.hpp>

#include <input/Event.hpp>
#include <input/Touch.hpp>

namespace Hyperion {

class UIStage;
class UIObject;
class UIImage;
class UIPanel;
class Scene;
class World;

/*! \brief Mobile touch controls subsystem for on-screen movement and look controls
 *
 *  Automatically creates:
 *  - Left side: Visual floating joystick for movement
 *  - Right side: Invisible touch area for camera look
 *
 *  Movement and look deltas are accessible via GetMovementDelta() and GetLookDelta()
 */
HYP_CLASS()
class HYP_API TouchControlsSubsystem final : public Subsystem
{
    HYP_OBJECT_BODY(TouchControlsSubsystem);

public:
    TouchControlsSubsystem();
    virtual ~TouchControlsSubsystem() override;

    TouchControlsSubsystem(const TouchControlsSubsystem& other) = delete;
    TouchControlsSubsystem& operator=(const TouchControlsSubsystem& other) = delete;

    /*! \brief Get the current movement input vector (-1 to 1 for each axis)
     *  \return Vec2f where x = strafe (left/right), y = forward/back */
    HYP_METHOD()
    Vec2f GetMovementDelta() const;

    /*! \brief Get the current look/camera input vector (-1 to 1 for each axis)
     *  \return Vec2f where x = yaw (left/right), y = pitch (up/down) */
    HYP_METHOD()
    Vec2f GetLookDelta() const;

    /*! \brief Check if any touch controls are currently active */
    HYP_METHOD()
    bool IsActive() const;

    /*! \brief Enable/disable touch controls */
    HYP_METHOD()
    void SetEnabled(bool enabled);

    /*! \brief Check if touch controls are enabled */
    HYP_METHOD()
    bool IsEnabled() const
    {
        return m_enabled;
    }

    /*! \brief Set the visual size of the movement joystick (in pixels) */
    HYP_METHOD()
    void SetJoystickSize(float size);

    /*! \brief Set the deadzone for joystick input (0.0 - 1.0, default 0.1) */
    HYP_METHOD()
    void SetDeadzone(float deadzone);

    /*! \brief Process touch input events - called by input system */
    void ProcessTouchEvent(const TouchEvent& touchEvent);

    /*! \brief Get touch information for a specific pointer ID
     *  \param pointerId The touch pointer ID
     *  \param outTouchPoint Output touch point information
     *  \return true if the touch is active, false otherwise */
    bool GetTouchPoint(int32 pointerId, TouchPoint& outTouchPoint) const;

    /*! \brief Check if a specific touch pointer is on the left side (movement) */
    bool IsTouchLeftSide(int32 pointerId) const;

    virtual void PreUpdate(float delta) override;
    virtual void Update(float delta) override;

protected:
    virtual SubsystemUpdatePhase GetUpdatePhase_Internal() const override
    {
        return SubsystemUpdatePhase::AfterVis;
    }

private:
    void Init() override;
    void OnAddedToWorld() override;
    void OnRemovedFromWorld() override;
    void OnSceneAttached(const Handle<Scene>& scene) override;
    void OnSceneDetached(Scene* scene) override;

    void CreateJoystickUI();
    void DestroyJoystickUI();
    void UpdateJoystickVisuals();
    void UpdateMovementFromTouch();
    void UpdateLookFromTouch();

    bool IsLeftSideOfScreen(const Vec2f& position) const;
    Vec2f NormalizeJoystickInput(const Vec2f& delta) const;
    void UpdateActiveTouches(float delta);

    // UI elements for movement joystick
    Handle<UIObject> m_joystickBase;
    Handle<UIObject> m_joystickKnob;

    // Touch tracking
    TMap<int32, TouchPoint> m_activeTouches;
    int32 m_leftTouchId = -1;   // Touch ID for left side (movement)
    int32 m_rightTouchId = -1;  // Touch ID for right side (look)

    // Current input values
    Vec2f m_movementDelta = Vec2f::Zero();
    Vec2f m_lookDelta = Vec2f::Zero();

    // Configuration
    bool m_enabled = true;
    float m_joystickSize = 120.0f;
    float m_knobSize = 50.0f;
    float m_deadzone = 0.1f;
    float m_maxJoystickRadius = 40.0f;
    float m_lookSensitivity = 2.0f;

    // Runtime state
    bool m_isInitialized = false;
    Vec2f m_screenSize = Vec2f::Zero();
};

} // namespace Hyperion
