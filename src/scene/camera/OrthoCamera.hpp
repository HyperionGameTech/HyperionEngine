#ifndef HYPERION_V2_ORTHO_CAMERA_H
#define HYPERION_V2_ORTHO_CAMERA_H

#include "Camera.hpp"

namespace hyperion::v2 {
class OrthoCameraController : public CameraController
{
public:
    OrthoCameraController();
    OrthoCameraController(float left, float right, float bottom, float top, float _near, float _far);
    virtual ~OrthoCameraController() override = default;

    virtual void OnAdded(Camera *camera) override;

    virtual void UpdateLogic(double dt) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;

protected:
    float m_left, m_right,
        m_bottom, m_top,
        m_near, m_far;
};
} // namespace hyperion::v2

#endif
