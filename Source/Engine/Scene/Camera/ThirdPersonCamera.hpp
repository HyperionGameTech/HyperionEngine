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

/*! \brief Orbits the camera around its parent node (usually the player), with the view direction
 *  driving camera-relative character movement. The camera ignores its parent's transform so the
 *  character can turn without dragging the view along with it. */
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

    /*! \brief World-space direction the camera is looking, independent of any pending transform updates. */
    HYP_METHOD()
    Vec3f GetViewDirection() const;

    virtual void UpdateLogic(double delta) override;

protected:
    virtual void Init() override;

    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
    virtual void OnRemoved() override;

    HYP_FIELD(Property = "PivotOffset", Serialize, Editor, Title = "Pivot Offset", Description = "Offset from the parent node's origin that the camera orbits around")
    Vec3f m_pivotOffset = Vec3f(0.0f, 1.6f, 0.0f);

    HYP_FIELD(Property = "ShoulderOffset", Serialize, Editor, Title = "Shoulder Offset", Description = "Sideways offset of the orbit pivot, for an over-the-shoulder view")
    float m_shoulderOffset = 0.55f;

    HYP_FIELD(Property = "Distance", Serialize, Editor, Title = "Distance")
    float m_distance = 2.1f;

    HYP_FIELD(Property = "MinDistance", Serialize, Editor, Title = "Min Distance")
    float m_minDistance = 1.2f;

    HYP_FIELD(Property = "MaxDistance", Serialize, Editor, Title = "Max Distance")
    float m_maxDistance = 4.5f;

    HYP_FIELD(Property = "Yaw", Serialize, Editor, Title = "Yaw", Description = "Orbit yaw in degrees; 0 looks down +Z")
    float m_yaw = 0.0f;

    HYP_FIELD(Property = "Pitch", Serialize, Editor, Title = "Pitch", Description = "Orbit pitch in degrees; positive looks down at the pivot")
    float m_pitch = 8.0f;

    HYP_FIELD(Property = "MinPitch", Serialize, Editor, Title = "Min Pitch")
    float m_minPitch = -40.0f;

    HYP_FIELD(Property = "MaxPitch", Serialize, Editor, Title = "Max Pitch")
    float m_maxPitch = 70.0f;

    HYP_FIELD(Property = "MinEyeHeight", Serialize, Editor, Title = "Min Eye Height", Description = "Lowest the camera may sit relative to the pivot; looking up pulls the camera in instead of dipping below this")
    float m_minEyeHeight = -1.3f;

    HYP_FIELD(Property = "FollowSharpness", Serialize, Editor, Title = "Follow Sharpness", Description = "How tightly the camera tracks the pivot; 0 disables smoothing")
    float m_followSharpness = 25.0f;

    HYP_FIELD(Property = "ZoomStep", Serialize, Editor, Title = "Zoom Step", Description = "Distance change per mouse wheel notch")
    float m_zoomStep = 0.5f;

    HYP_FIELD(Property = "MouseSensitivity", Serialize, Editor, Title = "Mouse Sensitivity")
    float m_mouseSensitivity = 120.0f;

private:
    Vec3f CalculatePivotTarget() const;

    void DetachFromParentTransform();
    void RestoreParentTransform();

    Vec3f m_smoothedPivot;
    bool m_hasSmoothedPivot = false;

    float m_currentDistance = 2.1f;

    bool m_addedIgnoreParentTransform = false;
};

} // namespace Hyperion
