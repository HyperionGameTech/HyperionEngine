/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Camera/FirstPersonCamera.hpp>

namespace Hyperion {

class EditorCameraController;

HYP_ENUM()
enum class EditorCameraControllerMode : uint32
{
    INACTIVE = 0,
    FOCUSED = 1,
    MOUSE_LOCKED = 2
};

HYP_CLASS()
class EditorCameraInputHandler : public InputHandlerBase
{
    HYP_OBJECT_BODY(EditorCameraInputHandler);

public:
    EditorCameraInputHandler();
    explicit EditorCameraInputHandler(EditorCameraController* controller);
    virtual ~EditorCameraInputHandler() override = default;

    virtual bool OnKeyDown(const KeyboardEvent& evt) override;
    virtual bool OnKeyUp(const KeyboardEvent& evt) override;
    virtual bool OnMouseDown(const MouseEvent& evt) override;
    virtual bool OnMouseUp(const MouseEvent& evt) override;
    virtual bool OnMouseMove(const MouseEvent& evt) override;
    virtual bool OnMouseDrag(const MouseEvent& evt) override;
    virtual bool OnMouseScroll(const MouseEvent& evt) override;
    virtual bool OnMouseLeave(const MouseEvent& evt) override;
    virtual bool OnClick(const MouseEvent& evt) override;
    virtual bool OnGainFocus(const MouseEvent& evt) override;
    virtual bool OnLoseFocus(const MouseEvent& evt) override;

private:
    EditorCameraController* m_controller;
};

HYP_CLASS()
class EditorCameraController : public FirstPersonCameraController
{
    HYP_OBJECT_BODY(EditorCameraController);

public:
    EditorCameraController();
    virtual ~EditorCameraController() = default;

    EditorCameraControllerMode GetMode() const
    {
        return m_mode;
    }

    void SetMode(EditorCameraControllerMode mode);

    static float GetMoveSpeed();
    static void SetMoveSpeed(float moveSpeed);
    static void ResetMoveSpeed();

    virtual void UpdateLogic(double delta) override;

protected:
    virtual void OnActivated() override;

    virtual Vec3f GetKeyboardMovement(double delta) const override;

    EditorCameraControllerMode m_mode;
};

HYP_ENUM()
enum class EditorViewportType : uint8
{
    Perspective = 0,
    Top,
    Front,
    Side
};

class EditorOrthoCameraController;

HYP_CLASS()
class EditorOrthoCameraInputHandler : public InputHandlerBase
{
    HYP_OBJECT_BODY(EditorOrthoCameraInputHandler);

public:
    EditorOrthoCameraInputHandler();
    explicit EditorOrthoCameraInputHandler(EditorOrthoCameraController* controller);
    virtual ~EditorOrthoCameraInputHandler() override = default;

    virtual bool OnMouseDrag(const MouseEvent& evt) override;
    virtual bool OnMouseScroll(const MouseEvent& evt) override;

private:
    EditorOrthoCameraController* m_controller;
};

HYP_CLASS()
class EditorOrthoCameraController : public CameraController
{
    HYP_OBJECT_BODY(EditorOrthoCameraController);

public:
    EditorOrthoCameraController();
    explicit EditorOrthoCameraController(EditorViewportType viewportType);
    virtual ~EditorOrthoCameraController() override = default;

    static Vec3f GetViewDirection(EditorViewportType viewportType);
    static Vec3f GetViewUp(EditorViewportType viewportType);
    static Vec3f GetViewSide(EditorViewportType viewportType);

    HYP_FORCE_INLINE EditorViewportType GetViewportType() const
    {
        return m_viewportType;
    }

    HYP_FORCE_INLINE float GetHalfHeight() const
    {
        return m_halfHeight;
    }

    void SetHalfHeight(float halfHeight);

    Vec2f GetWorldSize() const;

    void Pan(Vec2f screenDelta);
    void Zoom(float wheelSteps, Vec2f screenPosition);
    void CenterOn(const Vec3f& point);

    virtual void UpdateLogic(double delta) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;

protected:
    virtual void OnActivated() override;

    EditorViewportType m_viewportType;
    float m_halfHeight;
};

} // namespace Hyperion
