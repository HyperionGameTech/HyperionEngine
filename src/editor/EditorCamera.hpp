/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_CAMERA_HPP
#define HYPERION_EDITOR_CAMERA_HPP

#include <scene/camera/FirstPersonCamera.hpp>

#include <math/Vector2.hpp>

namespace hyperion {

enum EditorCameraControllerMode
{
    ECM_INACTIVE,
    ECM_FOCUSED
};

class HYP_API EditorCameraController : public FirstPersonCameraController
{
public:
    EditorCameraController();
    virtual ~EditorCameraController() = default;

    EditorCameraControllerMode GetMode() const
        { return m_mode; }

    void SetMode(EditorCameraControllerMode mode);

    virtual void UpdateLogic(double dt) override;

protected:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;

    EditorCameraControllerMode  m_mode;
};
} // namespace hyperion

#endif
