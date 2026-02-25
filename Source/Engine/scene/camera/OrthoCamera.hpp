/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/camera/Camera.hpp>

namespace Hyperion {

HYP_CLASS()
class OrthoCameraController : public CameraController
{
    HYP_OBJECT_BODY(OrthoCameraController);

public:
    OrthoCameraController();
    OrthoCameraController(float left, float right, float bottom, float top, float _near, float _far);
    virtual ~OrthoCameraController() override = default;

    virtual void UpdateLogic(double delta) override;
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

} // namespace Hyperion
