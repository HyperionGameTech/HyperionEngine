/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ORTHO_CAMERA_HPP
#define HYPERION_ORTHO_CAMERA_HPP

#include <scene/camera/Camera.hpp>

namespace hyperion {

HYP_CLASS()
class OrthoCameraController : public CameraController
{
    HYP_OBJECT_BODY(OrthoCameraController);

public:
    OrthoCameraController();
    OrthoCameraController(float left, float right, float bottom, float top, float _near, float _far);
    virtual ~OrthoCameraController() override = default;

    virtual void UpdateLogic(double dt) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;

    float m_left,
        m_right,
        m_bottom,
        m_top,
        m_near,
        m_far;
};

} // namespace hyperion

#endif
