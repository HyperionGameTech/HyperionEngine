/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_CAMERA_HPP
#define HYPERION_EDITOR_CAMERA_HPP

#include <scene/camera/FirstPersonCamera.hpp>

namespace hyperion {

class EditorCameraController;

HYP_ENUM()
enum class EditorCameraControllerMode : uint32
{
    INACTIVE        = 0,
    FOCUSED         = 1,
    MOUSE_LOCKED    = 2
};

HYP_CLASS()
class HYP_API EditorCameraInputHandler : public InputHandlerBase
{
    HYP_OBJECT_BODY(EditorCameraInputHandler);

public:
    EditorCameraInputHandler(CameraController *controller);
    virtual ~EditorCameraInputHandler() override = default;
    
protected:
    virtual bool OnKeyDown_Impl(const KeyboardEvent &evt) override;
    virtual bool OnKeyUp_Impl(const KeyboardEvent &evt) override;
    virtual bool OnMouseDown_Impl(const MouseEvent &evt) override;
    virtual bool OnMouseUp_Impl(const MouseEvent &evt) override;
    virtual bool OnMouseMove_Impl(const MouseEvent &evt) override;
    virtual bool OnMouseDrag_Impl(const MouseEvent &evt) override;
    virtual bool OnClick_Impl(const MouseEvent &evt) override;

private:
    EditorCameraController  *m_controller;
};

HYP_CLASS()
class HYP_API EditorCameraController : public FirstPersonCameraController
{
    HYP_OBJECT_BODY(EditorCameraController);

public:
    EditorCameraController();
    virtual ~EditorCameraController() = default;

    EditorCameraControllerMode GetMode() const
        { return m_mode; }

    void SetMode(EditorCameraControllerMode mode);

    virtual void UpdateLogic(double delta) override;

protected:
    virtual void OnActivated() override;

    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;

    EditorCameraControllerMode  m_mode;
};
} // namespace hyperion

#endif
