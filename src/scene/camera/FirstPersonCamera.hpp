/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FIRST_PERSON_CAMERA_HPP
#define HYPERION_FIRST_PERSON_CAMERA_HPP

#include <scene/camera/PerspectiveCamera.hpp>

#include <math/Vector2.hpp>

namespace hyperion {

enum class FirstPersonCameraControllerMode
{
    MOUSE_LOCKED,
    MOUSE_FREE
};

HYP_CLASS()
class HYP_API FirstPersonCameraController : public PerspectiveCameraController
{
    HYP_OBJECT_BODY(FirstPersonCameraController);

public:
    FirstPersonCameraController(FirstPersonCameraControllerMode mode = FirstPersonCameraControllerMode::MOUSE_FREE);
    virtual ~FirstPersonCameraController() = default;
    
    FirstPersonCameraControllerMode GetMode() const
        { return m_mode; }

    void SetMode(FirstPersonCameraControllerMode mode);

    virtual bool IsMouseLocked() const override
        { return m_mode == FirstPersonCameraControllerMode::MOUSE_LOCKED; }

    virtual void UpdateLogic(double dt) override;

protected:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;

    FirstPersonCameraControllerMode m_mode;

    Vec3f                           m_move_deltas;
    Vec3f                           m_dir_cross_y;

    float                           m_mouse_x;
    float                           m_mouse_y;
    float                           m_prev_mouse_x;
    float                           m_prev_mouse_y;
    
    Vec2f                           m_mag;
    Vec2f                           m_desired_mag;
    Vec2f                           m_prev_mag;
};

} // namespace hyperion

#endif
