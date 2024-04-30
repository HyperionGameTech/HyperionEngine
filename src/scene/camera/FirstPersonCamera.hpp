/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FIRST_PERSON_CAMERA_HPP
#define HYPERION_FIRST_PERSON_CAMERA_HPP

#include <scene/camera/PerspectiveCamera.hpp>

#include <math/Vector2.hpp>

namespace hyperion {

enum FirstPersonCameraControllerMode
{
    FPCCM_MOUSE_LOCKED,
    FPCCM_MOUSE_FREE
};

class HYP_API FirstPersonCameraController : public PerspectiveCameraController
{
public:
    FirstPersonCameraController(FirstPersonCameraControllerMode mode = FPCCM_MOUSE_FREE);
    virtual ~FirstPersonCameraController() = default;
    
    FirstPersonCameraControllerMode GetMode() const
        { return m_mode; }

    void SetMode(FirstPersonCameraControllerMode mode)
        { m_mode = mode; }

    virtual bool IsMouseLocked() const override
        { return m_mode == FPCCM_MOUSE_LOCKED; }

    virtual void UpdateLogic(double dt) override;

protected:
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
} // namespace hyperion

#endif
