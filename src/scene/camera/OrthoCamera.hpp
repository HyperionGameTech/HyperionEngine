#ifndef HYPERION_V2_ORTHO_CAMERA_H
#define HYPERION_V2_ORTHO_CAMERA_H

#include "Camera.hpp"

namespace hyperion::v2 {
class OrthoCameraController : public CameraController
{
public:
    OrthoCameraController();
    virtual ~OrthoCameraController() = default;

    virtual void UpdateLogic(double dt) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;
};
} // namespace hyperion::v2

#endif
