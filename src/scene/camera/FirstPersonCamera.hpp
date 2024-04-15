/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_FIRST_PERSON_CAMERA_H
#define HYPERION_V2_FIRST_PERSON_CAMERA_H

#include <math/Vector2.hpp>
#include "PerspectiveCamera.hpp"

namespace hyperion::v2 {

enum FirstPersonCameraControllerMode
{
    FPC_MODE_MOUSE_LOCKED,
    FPC_MODE_MOUSE_FREE
};

class HYP_API FirstPersonCameraController : public PerspectiveCameraController
{
public:
    FirstPersonCameraController(FirstPersonCameraControllerMode mode = FPC_MODE_MOUSE_FREE);
    virtual ~FirstPersonCameraController() = default;
    
    FirstPersonCameraControllerMode GetMode() const
        { return m_mode; }

    void SetMode(FirstPersonCameraControllerMode mode)
        { m_mode = mode; }

    virtual bool IsMouseLocked() const override
        { return m_mode == FPC_MODE_MOUSE_LOCKED; }

    virtual void UpdateLogic(double dt) override;

private:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;

    FirstPersonCameraControllerMode m_mode;

    Vector3 m_move_deltas,
            m_dir_cross_y;

    float m_mouse_x,
          m_mouse_y,
          m_prev_mouse_x,
          m_prev_mouse_y;
    
    Vector2 m_mag,
            m_desired_mag,
            m_prev_mag;
};
} // namespace hyperion::v2

#endif
