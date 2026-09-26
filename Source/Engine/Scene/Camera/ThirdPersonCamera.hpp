/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Camera/PerspectiveCamera.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Input/Event.hpp>

namespace Hyperion {

class ThirdPersonCameraController;

HYP_CLASS()
class ENGINE_API ThirdPersonCameraInputHandler : public InputHandlerBase
{
    HYP_OBJECT_BODY(ThirdPersonCameraInputHandler);

public:
    ThirdPersonCameraInputHandler();
    explicit ThirdPersonCameraInputHandler(ThirdPersonCameraController* controller);

    virtual ~ThirdPersonCameraInputHandler() override = default;

    virtual bool OnKeyDown(const KeyboardEvent& evt) override;

    virtual bool OnMouseDown(const MouseEvent& evt) override;
    virtual bool OnMouseUp(const MouseEvent& evt) override;
    virtual bool OnMouseMove(const MouseEvent& evt) override;
    virtual bool OnMouseScroll(const MouseEvent& evt) override;

    virtual bool OnTouchMove(const TouchEvent& evt) override;

    virtual bool OnGainFocus(const MouseEvent& evt) override;
    virtual bool OnLoseFocus(const MouseEvent& evt) override;

    virtual bool OnControllerButtonDown(ControllerButton btn) override;

private:
    bool ShouldProcessTouch(const TouchEvent& evt) const;

    ThirdPersonCameraController* m_controller;
};

HYP_CLASS()
class ENGINE_API ThirdPersonCameraController : public PerspectiveCameraController
{
    HYP_OBJECT_BODY(ThirdPersonCameraController);

public:
    ThirdPersonCameraController();
    virtual ~ThirdPersonCameraController() override = default;

    HYP_METHOD()
    virtual bool IsMouseLockAllowed() const override
    {
        return true;
    }

    HYP_METHOD()
    void SetMouseLocked(bool mouseLocked);

    HYP_METHOD()
    const Vec3f& GetPivotOffset() const
    {
        return m_pivotOffset;
    }

    HYP_METHOD()
    void SetPivotOffset(const Vec3f& pivotOffset)
    {
        m_pivotOffset = pivotOffset;
    }

    HYP_METHOD()
    float GetDistance() const
    {
        return m_distance;
    }

    HYP_METHOD()
    void SetDistance(float distance);

    HYP_METHOD()
    float GetMouseSensitivity() const
    {
        return m_mouseSensitivity;
    }

    HYP_METHOD()
    void AddYawPitch(float yawDegrees, float pitchDegrees);

    HYP_METHOD()
    void Zoom(float amount);

    HYP_METHOD()
    Vec3f GetViewDirection() const;

    virtual void UpdateLogic(double delta) override;

protected:
    virtual void Init() override;

    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
    virtual void OnRemoved() override;

    HYP_FIELD(Property = "PivotOffset", Serialize, Editor, Title = "Pivot Offset")
    Vec3f m_pivotOffset = Vec3f(0.0f, 1.8f, 0.0f);

    HYP_FIELD(Property = "ShoulderOffset", Serialize, Editor, Title = "Shoulder Offset")
    float m_shoulderOffset = 0.6f;

    HYP_FIELD(Property = "Distance", Serialize, Editor, Title = "Distance")
    float m_distance = 1.6f;

    HYP_FIELD(Property = "MinDistance", Serialize, Editor, Title = "Min Distance")
    float m_minDistance = 1.0f;

    HYP_FIELD(Property = "MaxDistance", Serialize, Editor, Title = "Max Distance")
    float m_maxDistance = 3.0f;

    HYP_FIELD(Property = "Yaw", Serialize, Editor, Title = "Yaw")
    float m_yaw = 0.0f;

    HYP_FIELD(Property = "Pitch", Serialize, Editor, Title = "Pitch")
    float m_pitch = 4.0f;

    HYP_FIELD(Property = "MinPitch", Serialize, Editor, Title = "Min Pitch")
    float m_minPitch = -40.0f;

    HYP_FIELD(Property = "MaxPitch", Serialize, Editor, Title = "Max Pitch")
    float m_maxPitch = 70.0f;

    HYP_FIELD(Property = "MinEyeHeight", Serialize, Editor, Title = "Min Eye Height")
    float m_minEyeHeight = -1.4f;

    HYP_FIELD(Property = "FollowSharpness", Serialize, Editor, Title = "Follow Sharpness")
    float m_followSharpness = 25.0f;

    HYP_FIELD(Property = "ZoomStep", Serialize, Editor, Title = "Zoom Step", Description = "Distance change per mouse wheel notch")
    float m_zoomStep = 0.5f;

    HYP_FIELD(Property = "MouseSensitivity", Serialize, Editor, Title = "Mouse Sensitivity")
    float m_mouseSensitivity = 120.0f;

    HYP_FIELD(Property = "ControllerYawSpeed", Serialize, Editor, Title = "Controller Yaw Speed")
    float m_controllerYawSpeed = 220.0f;

    HYP_FIELD(Property = "ControllerPitchSpeed", Serialize, Editor, Title = "Controller Pitch Speed")
    float m_controllerPitchSpeed = 130.0f;

    HYP_FIELD(Property = "ControllerDeadzone", Serialize, Editor, Title = "Controller Deadzone")
    float m_controllerDeadzone = 0.12f;

    HYP_FIELD(Property = "ControllerResponseExponent", Serialize, Editor, Title = "Controller Response Exponent")
    float m_controllerResponseExponent = 2.0f;

    HYP_FIELD(Property = "ControllerTurnBoost", Serialize, Editor, Title = "Controller Turn Boost")
    float m_controllerTurnBoost = 0.6f;

    HYP_FIELD(Property = "InvertControllerPitch", Serialize, Editor, Title = "Invert Controller Pitch")
    bool m_invertControllerPitch = false;

private:
    Vec3f CalculatePivotTarget() const;
    Vec2f UpdateControllerLookRate(const Vec2f& stick, float deltaSeconds);

    void DetachFromParentTransform();
    void RestoreParentTransform();

    Vec3f m_smoothedPivot;
    bool m_hasSmoothedPivot = false;

    float m_currentDistance = 1.6f;

    Vec2f m_controllerLookRate;
    float m_controllerTurnHoldTime = 0.0f;

    bool m_addedIgnoreParentTransform = false;
};

} // namespace Hyperion
