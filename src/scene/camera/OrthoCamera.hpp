#ifndef HYPERION_V2_ORTHO_CAMERA_H
#define HYPERION_V2_ORTHO_CAMERA_H

#include "Camera.hpp"

namespace hyperion::v2 {
class OrthoCameraController : public CameraController
{
public:
    OrthoCameraController();
    OrthoCameraController(Float left, Float right, Float bottom, Float top, Float _near, Float _far);
    virtual ~OrthoCameraController() = default;

    virtual void OnAdded(Camera *camera) override;

    virtual void UpdateLogic(double dt) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;

protected:
    Float m_left, m_right,
        m_bottom, m_top,
        m_near, m_far;
};
} // namespace hyperion::v2

#endif
