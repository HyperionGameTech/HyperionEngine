/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_CAMERA_HPP
#define HYPERION_EDITOR_CAMERA_HPP

#include <scene/camera/FirstPersonCamera.hpp>

namespace hyperion {

enum class EditorCameraControllerMode
{
    INACTIVE,
    FOCUSED,
    MOUSE_LOCKED
};

HYP_CLASS()
class HYP_API EditorCameraController : public FirstPersonCameraController
{
public:
    EditorCameraController();
    virtual ~EditorCameraController() = default;

    EditorCameraControllerMode GetMode() const
        { return m_mode; }

    void SetMode(EditorCameraControllerMode mode);

    virtual void UpdateLogic(double dt) override;

    virtual bool IsMouseLocked() const override
        { return m_mode == EditorCameraControllerMode::MOUSE_LOCKED; }

protected:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;

    EditorCameraControllerMode  m_mode;
};
} // namespace hyperion

#endif
