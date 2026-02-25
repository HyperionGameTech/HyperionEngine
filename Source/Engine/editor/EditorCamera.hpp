/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/camera/FirstPersonCamera.hpp>

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
class HYP_API EditorCameraInputHandler : public InputHandlerBase
{
    HYP_OBJECT_BODY(EditorCameraInputHandler);

public:
    EditorCameraInputHandler();
    explicit EditorCameraInputHandler(EditorCameraController* controller);
    virtual ~EditorCameraInputHandler() override = default;

protected:
    virtual bool OnKeyDown_Impl(const KeyboardEvent& evt) override;
    virtual bool OnKeyUp_Impl(const KeyboardEvent& evt) override;
    virtual bool OnMouseDown_Impl(const MouseEvent& evt) override;
    virtual bool OnMouseUp_Impl(const MouseEvent& evt) override;
    virtual bool OnMouseMove_Impl(const MouseEvent& evt) override;
    virtual bool OnMouseDrag_Impl(const MouseEvent& evt) override;
    virtual bool OnMouseLeave_Impl(const MouseEvent& evt) override;
    virtual bool OnClick_Impl(const MouseEvent& evt) override;
    virtual bool OnGainFocus_Impl(const MouseEvent& evt) override;
    virtual bool OnLoseFocus_Impl(const MouseEvent& evt) override;

private:
    EditorCameraController* m_controller;
};

HYP_CLASS()
class HYP_API EditorCameraController : public FirstPersonCameraController
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

    virtual void UpdateLogic(double delta) override;

protected:
    virtual void OnActivated() override;

    EditorCameraControllerMode m_mode;
};
} // namespace Hyperion
